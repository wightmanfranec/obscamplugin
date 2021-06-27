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

#include "FFMpegVideoDecoder.hpp"
#include <util/platform.h>

FFMpegVideoDecoder::~FFMpegVideoDecoder()
{
    shutdown();
    m_videoDecoder->free();
}

void FFMpegVideoDecoder::init()
{
    start();
}

void FFMpegVideoDecoder::input(const Packet packet, const int type, const int tag)
{
    m_queue.add(new PacketItem(packet, type, tag));
}

void FFMpegVideoDecoder::flush()
{
    while (m_queue.size() > 0)
    {
        m_queue.remove();
    }

    m_mutex.lock();
    // Re-initialize the decoder
    m_videoDecoder->free();
    m_mutex.unlock();
}

void FFMpegVideoDecoder::drain()
{
}

void FFMpegVideoDecoder::shutdown()
{
    m_queue.stop();
    join();
}

void FFMpegVideoDecoder::processPacketItem(PacketItem *packetItem)
{
    mMutex.lock();
    const uint64_t cur_time = os_gettime_ns();
    if (!m_videoDecoder->isValid())
    {
        if (m_videoDecoder->init(AV_CODEC_ID_H264) < 0)
        {
            blog(LOG_WARNING, "Could not initialize video decoder");
            return;
        }
    }

    const auto packet = packetItem->getPacket();
    const auto data = static_cast<unsigned char *>(packet.data());

    const auto ts = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    if (packetItem->getType() == 101)
    {
        bool got_output{false};
        const auto success = m_videoDecoder->decodeVideo(data, packet.size(), &ts,
                                                         &m_videoFrame, &got_output);
        if (!success)
        {
            blog(LOG_WARNING, "Error decoding video");
            m_mutex.unlock();
            return;
        }

        if (got_output && source)
        {
            m_videoFrame.timestamp = cur_time;
            obs_source_output_video(source, &m_videoFrame);
        }
    }
    m_mutex.unlock();
}

void *FFMpegVideoDecoder::run()
{
    while (!isStopped())
    {
        auto item = static_cast<PacketItem *>(m_queue.remove());
        if (item)
        {
            processPacketItem(item);
            delete item;
        }

        // Check queue lengths

        const std::size_t queueSizeThreshold{25};

        const auto queueSize = m_queue.size();
        if (queueSize > queueSizeThreshold)
        {
            blog(LOG_WARNING, "Video Decoding queue overloaded. %d frames behind. Please use a lower quality setting.", queueSize);

            if (queueSize > queueSizeThreshold)
            {
                while (m_queue.size() > 5)
                {
                    m_queue.remove();
                }
            }
        }
    }

    return nullptr;
}
