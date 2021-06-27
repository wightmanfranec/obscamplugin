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

#include <iostream>
#include <cstring>

#include "Portal.hpp"

namespace portal
{
    Portal::Portal(PortalDelegate *delegate)
        :
        m_delegate{delegate},
        m_listening{false}
    {
#if PORTAL_DEBUG_LOG_ENABLED
        libusbmuxd_set_debug_level(10);
#endif

        reloadDeviceList();
        startListeningForDevices();
    }

    void Portal::connectToDevice(Device::shared_ptr device)
    {
        if (m_device)
        {
            portal_log_stderr("%s: Disconnecting old device", __func__);

            m_device->disconnect();
            m_device = nullptr;
        }

        m_device = device;

        const auto productId = m_device->getProductId().c_str();
        const auto uuid = m_device->uuid().c_str();
        portal_log_stderr("PORTAL (%p): Connecting to device: %s (%s)", this, productId, uuid);

        // Connect to the device with the channel delegate.
        m_device->connect(1260, shared_from_this(), 2000);
    }

    void Portal::removeDisconnectedDevices()
    {
        std::list<Device::shared_ptr> devicesToRemove;

        // Get the list of disconnected devices
        for (const auto& deviceMap : m_devices)
        {
            const auto& device = deviceMap.second;
            if (!device->isConnected())
            {
                devicesToRemove.push_back(device);
            }
        }

        // Remove the unplugged devices.
        for (auto device : devicesToRemove)
        {
            removeDevice(device->m_device);
        }
    }

    void Portal::addConnectedDevices()
    {
        usbmuxd_device_info_t *devicelist{nullptr};
        const auto connectedDeviceCount = usbmuxd_get_device_list(&devicelist);
        if (connectedDeviceCount < 0)
        {
            portal_log_stderr("Failed to get device list!");
            return;
        }
        else if (connectedDeviceCount == 0)
        {
            portal_log_stderr("No devices attached!");
            return;
        }

        for (int i{0}; i < connectedDeviceCount; ++i)
        {
            addDevice(devicelist[i]);
        }
    }

    void Portal::reloadDeviceList()
    {
        removeDisconnectedDevices();
        addConnectedDevices();
    }

    // BUG: Listening for devices only works when there is one instance of the plugin
    int Portal::startListeningForDevices()
    {
        // Subscribe for device connections
        if (usbmuxd_subscribe(pt_usbmuxd_cb, this) != 0)
        {
            portal_log_stderr("Failed to listen/subscribe!");
            return -1;
        }

        m_listening = true;
        portal_log_stderr("%s: Listening for devices", __func__);

        return 0;
    }

    void Portal::stopListeningForDevices()
    {
        if (m_listening)
        {
            //Always returns 0
            usbmuxd_unsubscribe();
            m_listening = false;
        }
    }

    void Portal::addDevice(const usbmuxd_device_info_t &device)
    {
        // Filter out network connected devices
        if (strcmp(device.connection_type, "Network") == 0)
        {
            return;
        }

        if (m_devices.find(device.handle) == m_devices.end())
        {
            auto sp = Device::shared_ptr(std::make_shared<Device>(device));
            m_devices.insert(DeviceMap::value_type(device.handle, sp));

            portal_log_stderr("PORTAL (%p): Added device: %i (%s)", this, device.product_id, device.udid);
        }
    }

    void Portal::removeDevice(const usbmuxd_device_info_t &device)
    {
        auto it = m_devices.find(device.handle);
        if (it != m_devices.end())
        {
            it->second->disconnect();
            m_devices.erase(it);

            portal_log_stderr("PORTAL (%p): Removed device: %i (%s)", this, device.product_id, device.udid);
        }
    }

    void Portal::channel_onPacketReceive(const Packet packet, const int type, const int tag)
    {
        if (m_delegate)
        {
            m_delegate->portal_onDevicePacketReceive(packet, type, tag);
        }
    }

    void Portal::channel_onStop()
    {
        portal_log_stderr("Channel Did Stop in portal");
    }

    Portal::~Portal()
    {
        if (m_listening)
        {
            usbmuxd_unsubscribe();
        }
    }

    void pt_usbmuxd_cb(const usbmuxd_event_t *event, void *user_data)
    {
        auto client = static_cast<Portal*>(user_data);
        switch (event->event)
        {
        case UE_DEVICE_ADD:
            client->addDevice(event->device);
            break;

        case UE_DEVICE_REMOVE:
            client->removeDevice(event->device);
            break;
        }

        if (client->m_delegate)
        {
            client->m_delegate->portal_onDeviceListUpdate(client->m_devices);
        }
    }
} // namespace portal
