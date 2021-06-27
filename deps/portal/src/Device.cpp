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

#include <string>
#include <list>
#include <sstream>
#include <vector>
#include <algorithm>
#include "Device.hpp"

namespace portal
{
    Device::DeviceMap Device::m_devices{};

    Device::Device(const usbmuxd_device_info_t &device)
        :
        m_connected{false},
        m_device{device},
        m_uuid{m_device.udid},
        m_productId{std::to_string(m_device.product_id)}
    {
        m_devices[m_uuid].push_back(this);
        portal_log_stderr("Added %p to device list", this);
    }

    Device::Device(const Device &other)
        :
        m_connected{other.m_connected},
        m_device{other.m_device},
        m_uuid{other.m_uuid}
    {
        m_devices[m_uuid].push_back(this);
        portal_log_stderr("Added %p to device list (copy)", this);
    }

    Device::~Device()
    {
        disconnect();
        removeFromDeviceList();
        portal_log_stderr("Removed %p from device list", this);
    }

    Device &Device::operator=(const Device &rhs)
    {
        removeFromDeviceList();

        m_connected = rhs.m_connected;
        m_device = rhs.m_device;
        m_uuid = rhs.m_uuid;
        m_productId = rhs.m_productId;

        m_devices[m_uuid].push_back(this);

        return *this;
    }

    int Device::connect(const uint16_t port, std::shared_ptr<ChannelDelegate> channelDelegate, const int attempts)
    {
        const auto conn = usbmuxd_connect(m_device.handle, port);
        if (conn > 0)
        {
            m_connectedChannel = std::make_shared<Channel>(port, conn);
            m_connectedChannel->configureProtocolDelegate();
            m_connectedChannel->setDelegate(channelDelegate);
        }
        else
        {
            if (attempts > 0)
            {
                connect(port, channelDelegate, attempts - 1);
            }
        }

        return 0;
    }

    void Device::disconnect()
    {
        if (isConnected())
        {
            m_connectedChannel->close();
            m_connectedChannel = nullptr;
        }
    }

    bool Device::isConnected() const
    {
        return (m_connectedChannel != nullptr);
    }

    void Device::removeFromDeviceList()
    {
        auto &devs = m_devices[m_uuid];
        auto it = std::find(devs.begin(), devs.end(), this);
        if (it != devs.end())
        {
            devs.erase(it);
        }
    }

    std::ostream &operator<<(std::ostream &os, const Device &v)
    {
        os << "v.productName()" << " [ " << v.uuid() << " ]";
        return os;
    }
} // namespace portal
