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

#include "Channel.hpp"

namespace portal
{
    Channel::Channel(const int port, const int conn)
        : m_port{port},
          m_conn{conn},
          m_protocol{std::make_unique<SimpleDataPacketProtocol>()},
          m_running{StartInternalThread()}
    {
    }

    Channel::~Channel()
    {
        m_running = false;
        WaitForInternalThreadToExit();
        portal_log_stderr("%s: Deallocating", __func__);
    }

    void Channel::close()
    {
        m_running = false;
        WaitForInternalThreadToExit();
        usbmuxd_disconnect(m_conn);
    }

    bool Channel::StartInternalThread()
    {
        m_thread = std::thread(InternalThreadEntryFunc, this);
        return true;
    }

    void Channel::WaitForInternalThreadToExit()
    {
        if (m_thread.joinable())
        {
            m_thread.join();
        }
    }

    void Channel::StopInternalThread()
    {
        m_running = false;
    }

    void Channel::InternalThreadEntry()
    {
        while (m_running)
        {
            constexpr uint32_t numberOfBytesToAskFor = 65536; // (1 << 16); // This is the value in DarkLighting

            uint32_t numberOfBytesReceived = 0;
            char buffer[numberOfBytesToAskFor] = {0};

            const int ret = usbmuxd_recv_timeout(m_conn, (char *)&buffer, numberOfBytesToAskFor, &numberOfBytesReceived, 10);
            if (ret == 0)
            {
                if ((numberOfBytesReceived > 0) && m_running)
                {
                    m_protocol->processData((char *)buffer, numberOfBytesReceived);
                }
            }
            else
            {
                portal_log_stderr("There was an error receiving data");
                m_running = false;
            }
        }
    }

    void Channel::simpleDataPacketProtocolDelegate_onProcessPacket(const Packet packet, const int type, const int tag)
    {
        auto strongDelegate = m_delegate.lock();
        if (strongDelegate)
        {
            strongDelegate->channel_onPacketReceive(packet, type, tag);
        }
    }
} // namespace portal
