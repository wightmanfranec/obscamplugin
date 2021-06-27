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

#include <chrono>

#include "obs-iDevice-cam-source.hpp"
#include "FFMpegDecode.hpp"
#include "Queue.hpp"
#include "Thread.hpp"

class VideoDecoder final
{
public:
    VideoDecoder() = default;
    ~VideoDecoder() { m_decode.free(); }

    operator FFMpegDecode*() { return &m_decode; }
    FFMpegDecode* operator->() { return &m_decode; }

private:
    FFMpegDecode            m_decode{};
};

struct FFMpegVideoDecoderCallback
{
    virtual ~FFMpegVideoDecoderCallback() {}
};

class FFMpegVideoDecoder final : public VideoDecoder, private Thread
{
public:
    FFMpegVideoDecoder();
    ~FFMpegVideoDecoder();

    void init() override;
    void input(const Packet packet, const int type, const int tag) override;
    void flush() override;
    void drain() override;
    void shutdown() override;

    // Public data members

    obs_source_t*           m_source{nullptr};

private:
    // Data members

    WorkQueue<PacketItem*>  m_queue{};
    obs_source_frame        m_videoFrame{};
    VideoDecoder            m_videoDecoder{};
    std::mutex              m_mutex{};

    // Utility functions

    void *run() override;
    void processPacketItem(PacketItem *packetItem);
};
