/*
 portal
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

#include <cstdint>
#include <iostream>
#include <cstring>
#include "Protocol.hpp"

#ifdef WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

namespace portal
{
    SimpleDataPacketProtocol::SimpleDataPacketProtocol()
    {
        portal_log_stdout("SimpleDataPacketProtocol created");
    }

    SimpleDataPacketProtocol::~SimpleDataPacketProtocol()
    {
        m_buffer.clear();
        portal_log_stdout("SimpleDataPacketProtocol destroyed");
    }

    int SimpleDataPacketProtocol::processData(const char *data, const int dataLength)
    {
        if (dataLength > 0)
        {
            // Add data recieved to the end of buffer.
            m_buffer.insert(m_buffer.end(), data, data + dataLength);
        }

        // Ensure that the data inside the buffer is at least as big as the
        // length variable (32 bit int) and then read it out
        if (m_buffer.size() < sizeof(PortalFrame))
        {
            return -1;
        }

        // Read the portal frame out
        PortalFrame frame;
        memcpy(&frame, &m_buffer[0], sizeof(PortalFrame));

        uint32_t length = 0;
        length = ntohl(length);

        frame.version = ntohl(frame.version);
        frame.type = ntohl(frame.type);
        frame.tag = ntohl(frame.tag);
        frame.payloadSize = ntohl(frame.payloadSize);

        if (frame.payloadSize == 0)
        {
            portal_log_stdout("Payload is empty!");
            m_buffer.erase(m_buffer.begin(), m_buffer.begin() + sizeof(PortalFrame) + frame.payloadSize);
            return -1;
        }

        // Read payload size now
        // Check if we've got all the data for the packet

        if (m_buffer.size() <= (sizeof(PortalFrame) + frame.payloadSize))
        {
            // We haven't got the data for the packet just yet, so wait for next time!
            return -1;
        }

        // Read length bytes as that is the packet
        const auto first = m_buffer.begin() + sizeof(PortalFrame);
        const auto last = m_buffer.begin() + sizeof(PortalFrame) + frame.payloadSize;
        auto newVec = std::vector<char>(first, last);

        auto strongDelegate = m_delegate.lock();
        if (strongDelegate)
        {
            strongDelegate->simpleDataPacketProtocolDelegateDidProcessPacket(newVec, frame.type, frame.tag);
        }

        // Remove the data from buffer
        m_buffer.erase(m_buffer.begin(), m_buffer.begin() + sizeof(PortalFrame) + frame.payloadSize);

        // Attempt to parse another packet
        processData(nullptr, 0);

        return 0;
    }
} // namespace portal
