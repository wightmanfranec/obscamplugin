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

#pragma once


#include <obs.h>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4244)
#pragma warning(disable : 4204)
#endif

#include <libavcodec/avcodec.h>
#include <libavutil/log.h>

#ifdef _MSC_VER
#pragma warning(pop)
#endif

class FFMpegDecode final
{
public:
    FFMpegDecode() = default;
    ~FFMpegDecode() { free(); }

    int init(const AVCodecID id) noexcept;
    void free() noexcept;

    bool decodeAudio(const uint8_t* data, const std::size_t size,
                     obs_source_audio* audio,
                     bool* got_output) noexcept;

    bool decodeVideo(const uint8_t* data, const std::size_t size,
                     long long* ts,
                     obs_source_frame* frame,
                     bool* got_output) noexcept;

    bool isValid() const noexcept { return (m_decoder != nullptr); }

private:
    AVCodecContext*     m_decoder{nullptr};
    AVCodec*            m_codec{nullptr};
    AVFrame*            m_frame{nullptr};
    uint8_t*            m_packetBuffer{nullptr};
    std::size_t         m_packetSize{0};

    // Utility functions

    void copyData(const uint8_t *data, const std::size_t size) noexcept;
};
