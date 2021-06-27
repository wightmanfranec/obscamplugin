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
#include <vector>
#include <list>
#include <map>
#include <algorithm>

#include "logging.hpp"
#include "Device.hpp"

using portal_channel_receive_cb_t = void (*)(char *buffer, int buffer_len, void *user_data);

namespace portal
{
    using Packet = std::vector<char>;
    using DeviceMap = std::map<int, Device::shared_ptr>;

    struct PortalDelegate
    {
        virtual void portal_onDevicePacketReceive(const Packet packet, const int type, const int tag) = 0;
        virtual void portal_onDeviceListUpdate(const DeviceMap deviceMap) = 0;
        virtual ~PortalDelegate(){};
    };

    class Portal final : public ChannelDelegate, public std::enable_shared_from_this<Portal>
    {
    public:
        using shared_ptr = std::shared_ptr<Portal>;

        Portal(PortalDelegate* delegate);
        ~Portal();

        shared_ptr getptr() { return shared_from_this(); }

        int startListeningForDevices();
        void stopListeningForDevices();

        bool isListening() const noexcept { return m_listening; }

        void connectToDevice(Device::shared_ptr device);
        void reloadDeviceList();

        DeviceMap getDevices() const noexcept { return m_devices; }

        // Public data members

        PortalDelegate*         m_delegate{nullptr};
        Device::shared_ptr      m_device{nullptr};

    private:
        // Data members

        bool                    m_listening{false};
        DeviceMap               m_devices{};

        Portal(const Portal &other);
        Portal &operator=(const Portal &other);

        // Utility functions

        void removeDisconnectedDevices();
        void addConnectedDevices();

        void addDevice(const usbmuxd_device_info_t &device);
        void removeDevice(const usbmuxd_device_info_t &device);

        void channel_onPacketReceive(const Packet packet, const int type, const int tag);
        void channel_onStop();

        friend void pt_usbmuxd_cb(const usbmuxd_event_t *event, void *user_data);
    };
} // namespace portal
