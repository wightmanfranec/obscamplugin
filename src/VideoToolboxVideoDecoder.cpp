/*
 obs-iDevice-cam-source
Copyright (C) 2018-2019	Will Townsend <will@townsend.io>

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License along
 with this program. If not, see <https://www.gnu.org/licenses/>
 */

#include "VideoToolboxVideoDecoder.hpp"

#define NAL_LENGTH_PREFIX_SIZE 4

VideoToolboxDecoder::~VideoToolboxDecoder()
{
    shutdown();
}

void VideoToolboxDecoder::init()
{
    start();
}

void VideoToolboxDecoder::input(const Packet packet, const int type, const int tag)
{
    m_queue.add(new PacketItem(packet, type, tag));
}

void VideoToolboxDecoder::flush()
{
    while (m_queue.size() > 0)
    {
        m_queue.remove();
    }

    VTDecompressionSessionInvalidate(m_session);
    m_session = nullptr;
}

void VideoToolboxDecoder::drain()
{
}

void VideoToolboxDecoder::shutdown()
{
    m_queue.stop();

    if (m_session)
    {
        VTDecompressionSessionInvalidate(m_session);
        m_session = nullptr;
    }

    join();
}

void *VideoToolboxDecoder::run()
{
    while (!isStopped())
    {
        auto item = (PacketItem *)m_queue.remove();
        if (item)
        {
            processPacketItem(item);
            delete item;
        }
    }

    return nullptr;
}

void VideoToolboxDecoder::processPacketItem(PacketItem *packetItem)
{
    const auto packet = packetItem->getPacket();
    const uint32_t frameSize = packet.size();

    if (frameSize < 3)
    {
        return;
    }

    OSStatus status{0};

    const auto naluType = (packet[4] & 0x1F);
    if (naluType == 7 || naluType == 8)
    {
        // NALU is the SPS Parameter
        if (naluType == 7)
        {
            m_spsData = Packet(packet.begin() + 4, packet.end() + (frameSize - 4));
            m_waitingForSps = false;
            m_waitingForPps = true;
        }

        // NALU is the PPS Parameter
        if (naluType == 8)
        {
            m_ppsData = Packet(packet.begin() + 4, packet.end() + (frameSize - 4));
            m_waitingForPps = false;
        }

        if (m_waitingForPps || m_waitingForSps)
        {
            return;
        }

        const uint8_t *const parameterSetPointers[] = {(uint8_t *)m_spsData.data(), (uint8_t *)m_ppsData.data()};
        const size_t parameterSetSizes[] = {m_spsData.size(), m_ppsData.size()};

        status = CMVideoFormatDescriptionCreateFromH264ParameterSets(kCFAllocatorDefault,
                                                                     2, /* count of parameter sets */
                                                                     parameterSetPointers,
                                                                     parameterSetSizes,
                                                                     NAL_LENGTH_PREFIX_SIZE,
                                                                     &m_format);

        if (status != noErr)
        {
            blog(LOG_INFO, "Failed to create format description");
            m_format = nullptr;
        }
        else
        {
            if (!m_session)
            {
                createDecompressionSession();
            }
            else
            {
                const auto needNewDecompSession = (VTDecompressionSessionCanAcceptFormatDescription(m_session, m_format) == false);
                if (needNewDecompSession)
                {
                    blog(LOG_INFO, "Created Decompression session");
                    createDecompressionSession();
                }
            }
        }
    }

    // Ensure that
    if ((m_ppsData.size() < 1) ||
        (m_spsData.size() < 1))
    {
        return;
    }

    if (m_waitingForSps || m_waitingForPps)
    {
        return;
    }

    if (!m_format)
    {
        return;
    }

    // This decoder only supports these two frames
    if ((naluType != 1) &&
        (naluType != 5))
    {
        return;
    }

    if (!m_session)
    {
        createDecompressionSession();
    }

    // Create the sample data for the decoder

    CMBlockBufferRef blockBuffer{nullptr};
    long blockLength{0};

    // type 5 is an IDR frame NALU. The SPS and PPS NALUs should always be followed by an IDR (or IFrame) NALU, as far as I know
    if (naluType == 5)
    {
        blockLength = frameSize;

        // replace the start code header on this NALU with its size.
        // AVCC format requires that you do this.
        // htonl converts the unsigned int from host to network byte order
        const uint32_t dataLength32 = htonl(blockLength - 4);
        memcpy(packet.data(), &dataLength32, sizeof(uint32_t));

        // create a block buffer from the IDR NALU
        status = CMBlockBufferCreateWithMemoryBlock(nullptr,
                                                    packet.data(), // memoryBlock to hold buffered data
                                                    blockLength,         // block length of the mem block in bytes.
                                                    kCFAllocatorNull,
                                                    nullptr,
                                                    0,           // offsetToData
                                                    blockLength, // dataLength of relevant bytes, starting at offsetToData
                                                    0,
                                                    &blockBuffer);
    }

    // NALU type 1 is non-IDR (or PFrame) picture
    if (naluType == 1)
    {
        // non-IDR frames do not have an offset due to SPS and PSS, so the approach
        // is similar to the IDR frames just without the offset
        blockLength = frameSize;

        // again, replace the start header with the size of the NALU
        const uint32_t dataLength32 = htonl(blockLength - 4);
        memcpy(packet.data(), &dataLength32, sizeof(uint32_t));

        status = CMBlockBufferCreateWithMemoryBlock(nullptr,
                                                    packet.data(), // memoryBlock to hold data. If NULL, block will be alloc when needed
                                                    blockLength,         // overall length of the mem block in bytes
                                                    kCFAllocatorNull,
                                                    nullptr,
                                                    0,           // offsetToData
                                                    blockLength, // dataLength of relevant data bytes, starting at offsetToData
                                                    0,
                                                    &blockBuffer);
    }

    // now create our sample buffer from the block buffer,
    if (status != noErr)
    {
        // NSLog(@"Error creating block buffer: %@", @(status));
        return;
    }

    if (!blockBuffer)
    {
        return;
    }

    if (!m_format)
    {
        return;
    }

    // here I'm not bothering with any timing specifics since in this case we displayed all frames immediately
    CMSampleBufferRef sampleBuffer{nullptr};
    const size_t sampleSize = blockLength;
    status = CMSampleBufferCreate(kCFAllocatorDefault,
                                  blockBuffer,
                                  true,
                                  nullptr,
                                  nullptr,
                                  m_format,
                                  1,
                                  0,
                                  nullptr,
                                  1,
                                  &sampleSize,
                                  &sampleBuffer);

    VTDecodeFrameFlags flags{0};
    VTDecodeInfoFlags flagOut{};

    auto now = os_gettime_ns();

    VTDecompressionSessionDecodeFrame(m_session, sampleBuffer, flags,
                                      (void *)now, &flagOut);

    CFRelease(sampleBuffer);
}

void VideoToolboxDecoder::outputFrame(CVPixelBufferRef pixelBufferRef)
{
    auto image = pixelBufferRef;

    // obs_source_frame *frame = frame;
    // CMTime target_pts =
    // CMSampleBufferGetOutputPresentationTimeStamp(sampleBuffer);
    // CMTime target_pts_nano = CMTimeConvertScale(target_pts, NANO_TIMESCALE,
    // kCMTimeRoundingMethod_Default);
    // frame->timestamp = target_pts_nano.value;

    if (!updateFrame(source, &frame, image, m_format))
    {
        // Send blank video
        obs_source_output_video(source, nullptr);
        return;
    }

    obs_source_output_video(source, &frame);

    CVPixelBufferUnlockBaseAddress(image, kCVPixelBufferLock_ReadOnly);
}

bool VideoToolboxDecoder::updateFrame(obs_source_t* capture,
                                      obs_source_frame* frame,
                                      CVImageBufferRef imageBufferRef,
                                      CMVideoFormatDescriptionRef formatDesc)
{
    if (!formatDesc)
    {
        return false;
    }

    const auto dims = CMVideoFormatDescriptionGetDimensions(formatDesc);

    frame->timestamp = os_gettime_ns();
    frame->width = dims.width;
    frame->height = dims.height;
    frame->format = VIDEO_FORMAT_BGRA;
    // frame->format   = VIDEO_FORMAT_YUY2;

    CVPixelBufferLockBaseAddress(imageBufferRef, kCVPixelBufferLock_ReadOnly);

    if (!CVPixelBufferIsPlanar(imageBufferRef))
    {
        frame->linesize[0] = CVPixelBufferGetBytesPerRow(imageBufferRef);
        frame->data[0] = static_cast<uint8_t *>(CVPixelBufferGetBaseAddress(imageBufferRef));
        return true;
    }

    const auto count = CVPixelBufferGetPlaneCount(imageBufferRef);
    for (std::size_t i{0}; i < count; ++i)
    {
        frame->linesize[i] = CVPixelBufferGetBytesPerRowOfPlane(imageBufferRef, i);
        frame->data[i] = static_cast<uint8_t *>(CVPixelBufferGetBaseAddressOfPlane(imageBufferRef, i));
    }

    return true;
}

static void
DecompressionSessionDecodeFrameCallback(void* decompressionOutputRefCon,
                                        void* sourceFrameRefCon,
                                        OSStatus status,
                                        VTDecodeInfoFlags infoFlags,
                                        CVImageBufferRef imageBuffer,
                                        CMTime presentationTimeStamp,
                                        CMTime presentationDuration)
{
    auto decoder = static_cast<VideoToolboxDecoder *>(decompressionOutputRefCon);

    if ((status != noErr) || !imageBuffer)
    {
        blog(LOG_INFO, "VideoToolbox decoder returned no image");
    }
    else if (infoFlags & kVTDecodeInfo_FrameDropped)
    {
        blog(LOG_INFO, "VideoToolbox dropped frame");
    }

    decoder->outputFrame(imageBuffer);
}

void VideoToolboxDecoder::createDecompressionSession()
{
    m_session = nullptr;

    VTDecompressionOutputCallbackRecord callBackRecord{};
    callBackRecord.decompressionOutputCallback = DecompressionSessionDecodeFrameCallback;
    callBackRecord.decompressionOutputRefCon = this;

    // Destination Pixel Buffer Attributes
    auto destinationPixelBufferAttributes = CFDictionaryCreateMutable(nullptr, 0,
                                                                      &kCFTypeDictionaryKeyCallBacks,
                                                                      &kCFTypeDictionaryValueCallBacks);

    int val = kCVPixelFormatType_32BGRA;
    auto number = CFNumberCreate(nullptr, kCFNumberSInt32Type, &val);
    CFDictionarySetValue(destinationPixelBufferAttributes, kCVPixelBufferPixelFormatTypeKey, number);
    CFRelease(number);

    // Format Pixel Buffer Attributes
    auto videoDecoderSpecification = CFDictionaryCreateMutable(nullptr, 0,
                                                               &kCFTypeDictionaryKeyCallBacks,
                                                               &kCFTypeDictionaryValueCallBacks);

    CFDictionarySetValue(videoDecoderSpecification,
                         kVTVideoDecoderSpecification_EnableHardwareAcceleratedVideoDecoder,
                         kCFBooleanTrue);

    const auto err = VTDecompressionSessionCreate(nullptr, m_format,
                                                  videoDecoderSpecification,
                                                  destinationPixelBufferAttributes,
                                                  &callBackRecord,
                                                  &m_session);
    if (err != noErr)
    {
        blog(LOG_ERROR, "Failed creating Decompression session");
        return;
    }
}
