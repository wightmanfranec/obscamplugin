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

#include <usbmuxd.h>
#include <thread>

#include "logging.hpp"
#include "Protocol.hpp"

namespace portal
{
    struct ChannelDelegate
    {
        virtual void channel_onPacketReceive(std::vector<char> packet, const int type, const int tag) = 0;
        virtual void channel_onStop() = 0;
        virtual ~ChannelDelegate(){};
    };

    class Channel final : public SimpleDataPacketProtocolDelegate, public std::enable_shared_from_this<Channel>
    {
    public:
        Channel(const int port, const int conn);
        ~Channel();

        std::shared_ptr<Channel> getptr() { return shared_from_this(); }

        void close();

        void simpleDataPacketProtocolDelegate_onProcessPacket(const Packet packet, const int type, const int tag) override;

        void setDelegate(std::shared_ptr<ChannelDelegate> delegate) { m_delegate = delegate; }
        int getPort() const noexcept { return m_port; }
        void configureProtocolDelegate() { m_protocol->setDelegate(shared_from_this()); }

    private:
        // Data members

        int m_port{};
        int m_conn{};

        std::unique_ptr<SimpleDataPacketProtocol> m_protocol{nullptr};
        std::weak_ptr<ChannelDelegate> m_delegate{};

        bool m_running{false};
        std::thread m_thread;

        // Utility functions

        void setPacketDelegate(std::shared_ptr<SimpleDataPacketProtocolDelegate> delegate)
        {
            m_protocol->setDelegate(delegate);
        }

        bool StartInternalThread();
        void WaitForInternalThreadToExit();
        void StopInternalThread();
        void InternalThreadEntry();

        static void *InternalThreadEntryFunc(void *This)
        {
            ((portal::Channel *)This)->InternalThreadEntry();
            return nullptr;
        }
    };
} // namespace portal
