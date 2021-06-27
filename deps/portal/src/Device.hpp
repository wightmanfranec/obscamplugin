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

#include <map>
#include <string>
#include <vector>
#include <iostream>
#include <usbmuxd.h>

#include "logging.hpp"
#include "Protocol.hpp"
#include "Channel.hpp"

namespace portal
{
    class Portal;

    struct DeviceDelegate
    {
        virtual void deviceDidConnect() = 0;
        virtual void deviceDidDisconnect() = 0;
        virtual ~DeviceDelegate(){};
    };

    // Represents an iOS device detected by usbmuxd. This class can be used to
    // retrieve info and initiate TCP sessions with the iOS device.
    class Device final : public std::enable_shared_from_this<Device>
    {
    public:
        explicit Device(const usbmuxd_device_info_t &device);
        Device(const Device &other);
        ~Device();

        Device &operator=(const Device &rhs);

        std::shared_ptr<Device> getptr() { return shared_from_this(); }

        int connect(const uint16_t port, std::shared_ptr<ChannelDelegate> channelDelegate, const int attempts);

        bool isConnected() const;
        void disconnect();

        int usbmuxdHandle() const noexcept { return m_device.handle; }
        uint16_t productID() const noexcept { return m_device.product_id; }
        std::string uuid() const noexcept { return m_uuid; }
        std::string getProductId() const noexcept { return m_productId; }

        void setDelegate(DeviceDelegate *delegate) { m_delegate = delegate; }

        using DeviceMap = std::map<std::string, std::vector<Device*>>;
        using shared_ptr = std::shared_ptr<Device>;

    private:
        static DeviceMap            m_devices;

        // Data members

        DeviceDelegate*             m_delegate{nullptr};
        std::shared_ptr<Channel>    m_connectedChannel{nullptr};
        bool                        m_connected{false};
        usbmuxd_device_info_t       m_device{};
        std::string                 m_uuid{};
        std::string                 m_productId{};

        // Utility functions

        void removeFromDeviceList();

        // Friend functions

        friend class Portal;
        friend std::ostream &operator<<(std::ostream &os, const Device &v);
    };
} // namespace portal
