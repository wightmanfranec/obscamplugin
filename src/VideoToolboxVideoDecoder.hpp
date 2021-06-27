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

#ifndef VideoToolboxVideoDecoder_hpp
#define VideoToolboxVideoDecoder_hpp

#include <chrono>
#include <vector>
#include <obs.h>
#include <util/platform.h>
#include <VideoToolbox/VideoToolbox.h>

#include "Queue.hpp"
#include "Thread.hpp"
#include "Decoder.hpp"

class VideoToolboxDecoder final : public Decoder, private Thread
{
public:
    VideoToolboxDecoder() = default;
    ~VideoToolboxDecoder();

    void init() override;
    void input(const Packet packet, const int type, const int tag) override;
    void flush() override;
    void drain() override;
    void shutdown() override;

    void outputFrame(CVPixelBufferRef pixelBufferRef);
    bool updateFrame(obs_source_t* capture, obs_source_frame* frame,
                     CVImageBufferRef imageBufferRef,
                     CMVideoFormatDescriptionRef formatDesc);

    // Public data members

    // The OBS Source to update.
    obs_source_t*                   m_source{nullptr};

private:
    // Data members

    CMVideoFormatDescriptionRef     m_format{nullptr};
    VTDecompressionSessionRef       m_session{nullptr};

    bool                            m_waitingForSps{false};
    bool                            m_waitingForPps{false};

    Packet                          m_spsData{};
    Packet                          m_ppsData{};

    WorkQueue<PacketItem*>          m_queue{};
    obs_source_frame                m_frame{};

    // Utility functions

    void *run() override; // Thread callback
    void processPacketItem(PacketItem *packetItem);

    void createDecompressionSession();
};

#endif // VideoToolboxVideoDecoder_hpp
