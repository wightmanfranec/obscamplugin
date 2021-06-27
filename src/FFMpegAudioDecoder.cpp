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

#include <fstream>
#include <util/platform.h>
#include "FFMpegAudioDecoder.hpp"

FFMpegAudioDecoder::~FFMpegAudioDecoder()
{
    shutdown();
    m_audioDecoder->free();
}

void FFMpegAudioDecoder::init()
{
    start();
}

void FFMpegAudioDecoder::Input(const Packet packet, const int type, const int tag)
{
    m_queue.add(new PacketItem(packet, type, tag));
}

void FFMpegAudioDecoder::flush()
{
}

void FFMpegAudioDecoder::drain()
{
}

void FFMpegAudioDecoder::shutdown()
{
    m_queue.stop();
    join();
}

void FFMpegAudioDecoder::processPacketItem(PacketItem *packetItem)
{
    const uint64_t cur_time = os_gettime_ns();

    if (!m_audioDecoder->isValid())
    {
        if (m_audioDecoder->init(AV_CODEC_ID_AAC) < 0)
        {
            blog(LOG_WARNING, "Could not initialize audio decoder");
            return;
        }
    }

    const auto packet = packetItem->getPacket();
    const auto data = static_cast<unsigned char *>(packet.data());

    if (packetItem->getType() == 102)
    {
        bool got_output{false};
        const auto success = m_audioDecoder->decodeAudio(data, packet.size(), &m_audioFrame, &got_output);
        if (!success)
        {
            blog(LOG_WARNING, "Error decoding audio");
            return;
        }

        if (got_output && source)
        {
            m_audioFrame.timestamp = cur_time;
            obs_source_output_audio(source, &m_audioFrame);
        }
    }
}

void *FFMpegAudioDecoder::run()
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

        const int queueSize = m_queue.size();
        if (queueSize > queueSizeThreshold)
        {
            blog(LOG_WARNING, "Audio Decoding queue overloaded. %d frames behind. Please use a lower quality setting.", queueSize);

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
