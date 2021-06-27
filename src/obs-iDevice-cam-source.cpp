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

#include <obs-module.h>
#include <chrono>
#include <usbmuxd.h>
#include <obs-avc.h>

#include "Portal.hpp"
#include "FFMpegVideoDecoder.hpp"
#include "FFMpegAudioDecoder.hpp"
#ifdef __APPLE__
#include "VideoToolboxVideoDecoder.hpp"
#endif

#define TEXT_INPUT_NAME                 obs_module_text("IDEVICESCAM.Title")

#define SETTING_DEVICE_UUID             "setting_device_uuid"
#define SETTING_DEVICE_UUID_NONE_VALUE  "null"
#define SETTING_PROP_LATENCY            "latency"
#define SETTING_PROP_LATENCY_NORMAL     0
#define SETTING_PROP_LATENCY_LOW        1
#define SETTING_PROP_HARDWARE_DECODER   "setting_use_hw_decoder"

using namespace portal;

class IOSCameraInput final : public PortalDelegate
{
public:
    obs_source_t*           m_source{nullptr};
    obs_data_t*             m_settings{nullptr};
    bool                    m_active{false};
    obs_source_frame        m_frame{};
    std::string             m_deviceUUID{};
    Portal::shared_ptr      m_sharedPortal{nullptr};
    Portal                  m_portal{};
    VideoDecoder*           m_videoDecoder{nullptr};

#ifdef __APPLE__
    VideoToolboxDecoder     m_videoToolboxVideoDecoder{};
#endif

    FFMpegVideoDecoder      m_ffmpegVideoDecoder{};
    FFMpegAudioDecoder      m_ffmpegAudioDecoder{};

    IOSCameraInput(obs_source_t* source, obs_data_t* settings)
        :
        m_source{source},
        m_settings{settings},
        m_portal{this}
    {
        blog(LOG_INFO, "Creating instance of plugin!");

        // In order for the internal Portal Delegates to work there must be a
        // shared_ptr to the instance of Portal.
        //
        // We create a shared pointer to the heap allocated Portal instance, and
        // wrap it up in a sharedPointer with a deleter that doesn't do anything
        // (this is handled automatically with the class)
        const auto null_deleter = [](portal::Portal *portal) { UNUSED_PARAMETER(portal); };
        const auto portalReference = std::shared_ptr<portal::Portal>(&portal, null_deleter);
        m_sharedPortal = portalReference;

#ifdef __APPLE__
        m_videoToolboxVideoDecoder.m_source = m_source;
        m_videoToolboxVideoDecoder.init();
#endif

        m_ffmpegVideoDecoder.m_source = m_source;
        m_ffmpegVideoDecoder.init();

        m_ffmpegAudioDecoder.m_source = m_source;
        m_ffmpegAudioDecoder.init();

        m_videoDecoder = &m_ffmpegVideoDecoder;

        loadSettings(m_settings);
        m_active = true;
    }

    ~IOSCameraInput() = default;

    void activate()
    {
        blog(LOG_INFO, "Activating");
        m_active = true;
    }

    void deactivate()
    {
        blog(LOG_INFO, "Deactivating");
        m_active = false;
    }

    void loadSettings(obs_data_t* settings)
    {
        const auto device_uuid = obs_data_get_string(settings, SETTING_DEVICE_UUID);

        blog(LOG_INFO, "Loaded Settings: Connecting to device");
        connectToDevice(device_uuid, false);
    }

    void reconnectToDevice()
    {
        if (m_deviceUUID.size() >= 1)
        {
            connectToDevice(m_deviceUUID, true);
        }
    }

    void connectToDevice(const std::string uuid, const bool force)
    {
        if (m_portal.m_device)
        {
            // Make sure that we're not already connected to the device
            if ((force == false) &&
                (m_portal.m_device->uuid().compare(uuid) == 0) &&
                m_portal.m_device->isConnected())
            {
                blog(LOG_DEBUG, "Already connected to the device. Skipping.");
                return;
            }

            // Disconnect from from the old device
            m_portal.m_device->disconnect();
            m_portal.m_device = nullptr;
        }

        blog(LOG_INFO, "Connecting to device");

        m_ffmpegVideoDecoder.flush();

#ifdef __APPLE__
        m_videoToolboxVideoDecoder.Flush();
#endif

        // Find device
        auto devices = m_portal.getDevices();
        m_deviceUUID = std::string(uuid);

        for (const auto& [_, device] : devices)
        {
            const auto device_uuid = device->uuid();
            if (device_uuid.compare(uuid) == 0)
            {
                blog(LOG_DEBUG, "comparing \n%s\n%s\n", device_uuid.c_str(), uuid.c_str());
                m_portal.connectToDevice(device);
            }
        }
    }

    void portal_onDevicePacketReceive(std::vector<char> packet, const int type, const int tag)
    {
        enum class PacketType { Video = 101, Audio = 102 };

        try
        {
            switch (type)
            {
            case PacketType::Video:
                m_videoDecoder->input(packet, type, tag);
                break;

            case PacketType::Audio:
                m_ffmpegAudioDecoder.input(packet, type, tag);
                break;

            default:
                break;
            }
        }
        catch (...)
        {
            // This isn't great, but I haven't been able to figure out what is
            // causing the exception that happens when the phone is plugged in
            // with the app open OBS Studio is launched with the iOS Camera
            // plugin ready. This also doesn't happen _all_ the time. Which
            // makes this 'fun'.

            blog(LOG_INFO, "Exception caught...");
        }
    }

    void portal_onDeviceListUpdate(const DeviceMap deviceMap)
    {
        // Update OBS Settings
        blog(LOG_INFO, "Updated device list");

        // If there is one device in the list, then we should attempt to connect
        // to it. I would guess that this is the main use case - one device, and
        // it's good to attempt to automatically connect in this case, and 'just
        // work'.
        //
        // If there are multiple devices, then we can't just connect to all
        // devices. We cannot currently detect if a device is connected to
        // another instance of the plugin, so it's not safe to attempt to
        // connect to any devices automatically as we could be connecting to a
        // device that is currently connected elsewhere. Due to this, if there
        // are multiple devices, we won't do anything and will let the user
        // configure the instance of the plugin.
        if (deviceList.size() == 1)
        {
            for (const auto& [_, device] : deviceMap)
            {
                const auto uuid = device.get()->uuid();
                const auto isFirstTimeConnection = m_deviceUUID.empty();
                const auto isDeviceConnected = device.get()->isConnected();
                const auto isAlreadyConnectedDevice = (m_deviceUUID.compare(uuid) == 0);

                if (isFirstTimeConnection ||
                    (isAlreadyConnectedDevice && !isDeviceConnected))
                {
                    // Set the setting so that the UI in OBS Studio is updated
                    obs_data_set_string(this->m_settings, SETTING_DEVICE_UUID, uuid.c_str());

                    connectToDevice(uuid, false);
                }
            }
        }
        else
        {
            // User will have to configure the plugin manually when more than
            // one device is plugged in due to the fact that multiple instances
            // of the plugin can't subscribe to device events...
        }
    }
};

#pragma mark - Settings Config

static bool refereshDevices(obs_properties_t* props, obs_property_t* p, void* data)
{
    UNUSED_PARAMETER(p);

    auto cameraInput = reinterpret_cast<IOSCameraInput *>(data);

    cameraInput->m_portal.reloadDeviceList();
    auto devices = cameraInput->m_portal.getDevices();

    auto dev_list = obs_properties_get(props, SETTING_DEVICE_UUID);
    obs_property_list_clear(dev_list);

    obs_property_list_add_string(dev_list, "None", SETTING_DEVICE_UUID_NONE_VALUE);

    for (const auto& [_, device] : devices)
    {
        // Add the device uuid to the list. It would be neat to grab the device
        // name somehow, but that will likely require libmobiledevice instead of
        // usbmuxd. Something to look into.
        const auto uuid = device->uuid().c_str();
        obs_property_list_add_string(dev_list, uuid, uuid);
    }

    return true;
}

static bool reconnectToDevice(obs_properties_t* props, obs_property_t* p, void* data)
{
    UNUSED_PARAMETER(props);
    UNUSED_PARAMETER(p);

    auto cameraInput = reinterpret_cast<IOSCameraInput *>(data);
    cameraInput->reconnectToDevice();

    return false;
}

#pragma mark - Plugin Callbacks

static const char *getIosCameraInputName(void *)
{
    return TEXT_INPUT_NAME;
}

static void *createIosCameraInputputName(obs_data_t *settings, obs_source_t *source)
{
    IOSCameraInput* cameraInput = nullptr;

    try
    {
        cameraInput = new IOSCameraInput(source, settings);
    }
    catch (const char *error)
    {
        blog(LOG_ERROR, "Could not create device '%s': %s", obs_source_get_name(source), error);
    }

    return cameraInput;
}

static void destroyIosCameraInput(void* data)
{
    delete reinterpret_cast<IOSCameraInput *>(data);
}

static void deactivateIosCameraInput(void* data)
{
    auto cameraInput = reinterpret_cast<IOSCameraInput *>(data);
    cameraInput->deactivate();
}

static void activateIosCameraInput(void* data)
{
    auto cameraInput = reinterpret_cast<IOSCameraInput *>(data);
    cameraInput->activate();
}

static obs_properties_t *getIosCameraProperties(void* data)
{
    UNUSED_PARAMETER(data);

    auto ppts = obs_properties_create();

    auto dev_list = obs_properties_add_list(ppts, SETTING_DEVICE_UUID,
                                            "iOS Device",
                                            OBS_COMBO_TYPE_LIST,
                                            OBS_COMBO_FORMAT_STRING);

    obs_property_list_add_string(dev_list, "", "");

    refereshDevices(ppts, dev_list, data);

    obs_properties_add_button(ppts, "setting_refresh_devices", "Refresh Devices", refereshDevices);
    obs_properties_add_button(ppts, "setting_button_connect_to_device", "Reconnect to Device", reconnectToDevice);

    auto latency_modes = obs_properties_add_list(ppts, SETTING_PROP_LATENCY,
                                                 obs_module_text("IDEVICESCAM.Settings.Latency"),
                                                 OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);

    obs_property_list_add_int(latency_modes,
                              obs_module_text("IDEVICESCAM.Settings.Latency.Normal"),
                              SETTING_PROP_LATENCY_NORMAL);

    obs_property_list_add_int(latency_modes,
                              obs_module_text("IDEVICESCAM.Settings.Latency.Low"),
                              SETTING_PROP_LATENCY_LOW);

#ifdef __APPLE__
    obs_properties_add_bool(ppts, SETTING_PROP_HARDWARE_DECODER,
                            obs_module_text("IDEVICESCAM.Settings.UseHardwareDecoder"));
#endif

    return ppts;
}

static void getIosCameraDefaults(obs_data_t* settings)
{
    obs_data_set_default_string(settings, SETTING_DEVICE_UUID, "");
    obs_data_set_default_int(settings, SETTING_PROP_LATENCY, SETTING_PROP_LATENCY_LOW);
#ifdef __APPLE__
    obs_data_set_default_bool(settings, SETTING_PROP_HARDWARE_DECODER, false);
#endif
}

static void saveIosCameraInput(void* data, obs_data_t* settings)
{
    UNUSED_PARAMETER(data);
    UNUSED_PARAMETER(settings);
}

static void updateIosCameraInput(void* data, obs_data_t* settings)
{
    auto input = reinterpret_cast<IOSCameraInput *>(data);

    // Clear the video frame when a setting changes
    obs_source_output_video(input->m_source, nullptr);

    // Connect to the device
    const auto uuid = obs_data_get_string(settings, SETTING_DEVICE_UUID);
    input->connectToDevice(uuid, false);

    const auto is_unbuffered = (obs_data_get_int(settings, SETTING_PROP_LATENCY) == SETTING_PROP_LATENCY_LOW);
    obs_source_set_async_unbuffered(input->m_source, is_unbuffered);

#ifdef __APPLE__
    bool useHardwareDecoder = obs_data_get_bool(settings, SETTING_PROP_HARDWARE_DECODER);
    if (useHardwareDecoder)
    {
        input->m_videoDecoder = &input->m_videoToolboxVideoDecoder;
    }
    else
    {
        input->m_videoDecoder = &input->m_ffmpegVideoDecoder;
    }
#endif
}

void RegisterIOSCameraSource()
{
    obs_source_info info = {};

    info.id              = "idevices-source";
    info.type            = OBS_SOURCE_TYPE_INPUT;
    info.output_flags    = OBS_SOURCE_ASYNC_VIDEO | OBS_SOURCE_AUDIO;

    info.get_name        = getIosCameraInputName;
    info.create          = createIosCameraInputputName;
    info.destroy         = destroyIosCameraInput;
    info.deactivate      = deactivateIosCameraInput;
    info.activate        = activateIosCameraInput;
    info.get_defaults    = getIosCameraDefaults;
    info.get_properties  = getIosCameraProperties;
    info.save            = saveIosCameraInput;
    info.update          = updateIosCameraInput;

    obs_register_source(&info);
}
