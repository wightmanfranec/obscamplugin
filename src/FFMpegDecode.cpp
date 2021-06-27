/******************************************************************************
 Copyright (C) 2014 by Hugh Bailey <obs.jim@gmail.com>

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ******************************************************************************/

#include "FFMpegDecode.hpp"
#include "obs-ffmpeg-compat.h"
#include <obs-avc.h>

int FFMpegDecode::init(const AVCodecID id) noexcept
{
    m_codec = avcodec_find_decoder(id);
    if (!m_codec)
    {
        return -1;
    }

    m_decoder = avcodec_alloc_context3(m_codec);

    const auto ret = avcodec_open2(m_decoder, m_codec, nullptr);
    if (ret < 0)
    {
        free();
        return ret;
    }

    if (m_codec->capabilities & CODEC_CAP_TRUNC)
    {
        m_decoder->flags |= CODEC_FLAG_TRUNC;
    }

    m_decoder->flags |= AV_CODEC_FLAG_LOW_DELAY;
    m_decoder->flags2 = AV_CODEC_FLAG2_CHUNKS;

    return 0;
}

void FFMpegDecode::free() noexcept
{
    if (m_decoder)
    {
        avcodec_close(m_decoder);
        av_free(m_decoder);
        m_decoder = nullptr;
    }

    if (m_frame)
    {
        av_free(m_frame);
        m_frame = nullptr;
    }

    if (m_packetBuffer)
    {
        bfree(m_packetBuffer);
        m_packetBuffer = nullptr;
    }
}

static inline video_format convertPixelFormat(const int f)
{
    switch (f)
    {
    case AV_PIX_FMT_NONE:
        return VIDEO_FORMAT_NONE;
    case AV_PIX_FMT_YUV420P:
        return VIDEO_FORMAT_I420;
    case AV_PIX_FMT_NV12:
        return VIDEO_FORMAT_NV12;
    case AV_PIX_FMT_YUYV422:
        return VIDEO_FORMAT_YUY2;
    case AV_PIX_FMT_UYVY422:
        return VIDEO_FORMAT_UYVY;
    case AV_PIX_FMT_RGBA:
        return VIDEO_FORMAT_RGBA;
    case AV_PIX_FMT_BGRA:
        return VIDEO_FORMAT_BGRA;
    case AV_PIX_FMT_BGR0:
        return VIDEO_FORMAT_BGRX;
    case AV_PIX_FMT_YUVJ420P:
        return VIDEO_FORMAT_I420;
    default:
        return VIDEO_FORMAT_NONE;
    }
}

static inline audio_format convertSampleFormat(const int f)
{
    switch (f)
    {
    case AV_SAMPLE_FMT_U8:
        return AUDIO_FORMAT_U8BIT;
    case AV_SAMPLE_FMT_S16:
        return AUDIO_FORMAT_16BIT;
    case AV_SAMPLE_FMT_S32:
        return AUDIO_FORMAT_32BIT;
    case AV_SAMPLE_FMT_FLT:
        return AUDIO_FORMAT_FLOAT;
    case AV_SAMPLE_FMT_U8P:
        return AUDIO_FORMAT_U8BIT_PLANAR;
    case AV_SAMPLE_FMT_S16P:
        return AUDIO_FORMAT_16BIT_PLANAR;
    case AV_SAMPLE_FMT_S32P:
        return AUDIO_FORMAT_32BIT_PLANAR;
    case AV_SAMPLE_FMT_FLTP:
        return AUDIO_FORMAT_FLOAT_PLANAR;
    default:
        return AUDIO_FORMAT_UNKNOWN;
    }
}

static inline speaker_layout convertSpeakerLayout(const uint8_t channels)
{
    switch (channels)
    {
    case 0:
        return SPEAKERS_UNKNOWN;
    case 1:
        return SPEAKERS_MONO;
    case 2:
        return SPEAKERS_STEREO;
    case 3:
        return SPEAKERS_2POINT1;
    case 4:
        return SPEAKERS_4POINT0;
    case 5:
        return SPEAKERS_4POINT1;
    case 6:
        return SPEAKERS_5POINT1;
    case 8:
        return SPEAKERS_7POINT1;
    default:
        return SPEAKERS_UNKNOWN;
    }
}

void FFMpegDecode::copyData(const uint8_t *data, const std::size_t size) noexcept
{
    const auto newSize = size + INPUT_BUFFER_PADDING_SIZE;

    if (m_packetSize < newSize)
    {
        m_packetBuffer = brealloc(m_packetBuffer, newSize);
        m_packetSize = newSize;
    }

    memset(m_packetBuffer + size, 0, INPUT_BUFFER_PADDING_SIZE);
    memcpy(m_packetBuffer, data, size);
}

bool FFMpegDecode::decodeAudio(const uint8_t* data, const std::size_t size,
                               obs_source_audio* audio,
                               bool* got_output) noexcept
{
    *got_output = false;

    copyData(data, size);

    AVPacket packet = {0};
    av_init_packet(&packet);
    packet.data = m_packetBuffer;
    packet.size = (int)size;

    if (!m_frame)
    {
        m_frame = av_frame_alloc();
        if (!m_frame)
        {
            return false;
        }
    }

    int ret{0};

    if (data && size)
    {
        ret = avcodec_send_packet(m_decoder, &packet);
    }

    if (ret == 0)
    {
        ret = avcodec_receive_frame(m_decoder, m_frame);
    }

    const auto got_frame = (ret == 0);

    if ((ret == AVERROR_EOF) ||
        (ret == AVERROR(EAGAIN)))
    {
        ret = 0;
    }

    if (ret < 0)
    {
        return false;
    }
    else if (!got_frame)
    {
        return true;
    }

    for (std::size_t i{0}; i < MAX_AV_PLANES; ++i)
    {
        audio->data[i] = m_frame->data[i];
    }

    audio->samples_per_sec = m_frame->sample_rate;
    audio->format = convertSampleFormat(m_frame->format);
    audio->speakers = convertSpeakerLayout((uint8_t)m_decoder->channels);

    audio->frames = m_frame->nb_samples;

    if (audio->format == AUDIO_FORMAT_UNKNOWN)
    {
        return false;
    }

    *got_output = true;
    return true;
}

bool FFMpegDecode::decodeVideo(uint8_t* data, const std::size_t size,
                               long long* ts,
                               obs_source_frame* frame,
                               bool* got_output) noexcept
{
    *got_output = false;

    copyData(data, size);

    AVPacket packet = {0};
    av_init_packet(&packet);
    packet.data = m_packetBuffer;
    packet.size = (int)size;
    packet.pts = *ts;

    if (m_codec->id == AV_CODEC_ID_H264 && obs_avc_keyframe(data, size))
    {
        packet.flags |= AV_PKT_FLAG_KEY;
    }

    if (!m_frame)
    {
        m_frame = av_frame_alloc();
        if (!m_frame)
        {
            return false;
        }
    }

    auto ret = avcodec_send_packet(m_decoder, &packet);
    if (ret == 0)
    {
        ret = avcodec_receive_frame(m_decoder, m_frame);
    }

    const auto got_frame = (ret == 0);

    if ((ret == AVERROR_EOF) ||
        (ret == AVERROR(EAGAIN)))
    {
        ret = 0;
    }

    if (ret < 0)
    {
        return false;
    }
    else if (!got_frame)
    {
        return true;
    }

    for (std::size_t i{0}; i < MAX_AV_PLANES; ++i)
    {
        frame->data[i] = m_frame->data[i];
        frame->linesize[i] = m_frame->linesize[i];
    }

    const auto newFormat = convertPixelFormat(m_frame->format);
    if (newFormat != frame->format)
    {
        frame->format = newFormat;
        frame->full_range = (m_frame->color_range == AVCOL_RANGE_JPEG);

        const auto range = (frame->full_range ? VIDEO_RANGE_FULL : VIDEO_RANGE_PARTIAL);

        const auto success = video_format_get_parameters(VIDEO_CS_601, range,
                                                         frame->color_matrix,
                                                         frame->color_range_min,
                                                         frame->color_range_max);
        if (!success)
        {
            blog(LOG_ERROR, "Failed to get video format "
                            "parameters for video format %u",
                 VIDEO_CS_601);
            return false;
        }
    }

    *ts = m_frame->pts;

    frame->width = m_frame->width;
    frame->height = m_frame->height;
    frame->flip = false;

    if (frame->format == VIDEO_FORMAT_NONE)
    {
        return false;
    }

    *got_output = true;
    return true;
}
