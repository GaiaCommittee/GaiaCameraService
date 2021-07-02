#include "DahengServer.hpp"

#include <thread>
#include "GxIAPI.h"
#include "DxImageProc.h"

namespace Gaia::CameraService
{
    /// 相机离线回调
    void CameraCaptureCallback(void* parameters_package)
    {
        auto* parameters =  static_cast<GX_FRAME_CALLBACK_PARAM*>(parameters_package);
        auto* target = static_cast<DahengServer*>(parameters->pUserParam);
        if (target)
        {
            target->OnPictureCapture(const_cast<void *>(parameters->pImgBuf),
                                     parameters->nWidth, parameters->nHeight);
        }
    }

    /// Connect to the Redis server.
    DahengServer::DahengServer(unsigned int device_index,
                               unsigned int port, const std::string &ip,
                               const std::string& format) :
       DeviceIndex(device_index), DeviceName("daheng_camera." + std::to_string(device_index))
    {
        sw::redis::ConnectionOptions connection_options;
        connection_options.socket_timeout = std::chrono::milliseconds(10);
        connection_options.host = ip;
        connection_options.port = static_cast<int>(port);
        connection_options.type = sw::redis::ConnectionType::TCP;
        Connection = std::make_shared<sw::redis::Redis>(connection_options);
        Subscriber = std::make_unique<sw::redis::Subscriber>(Connection->subscriber());
        Subscriber->subscribe("cameras/" + DeviceName + "/command");
        Subscriber->on_message([this](const std::string& channel, const std::string& value){
           this->HandleCommand(value);
        });

        Logger = std::make_unique<LogService::LogClient>(Connection);
        Logger->Author = DeviceName;

        Configurator = std::make_unique<ConfigurationService::ConfigurationClient>(DeviceName, Connection);

        if (format == "bayer_rg")
        {
            DeviceFormat = BAYERBG;
        }
        else if (format == "bayer_gr")
        {
            DeviceFormat = BAYERGB;
        }
        else
        {
            DeviceFormat = BAYERBG;
            Logger->RecordError("Unsupported color format '" + format + "', use 'bayer_rg' by default.");
        }
    }

    /// Stop the updater if it's still running.
    DahengServer::~DahengServer()
    {
        // Close camera device.
        Close();
        // Save configuration.
        Configurator->Apply();
    }

    /// Invoked when a new picture is captured by the camera.
    void DahengServer::OnPictureCapture(void *data, int width, int height)
    {
        ReceivedPicturesCount++;
        DxRaw8toRGB24(const_cast<void *>(data), PictureSource.GetPointer(),
                      static_cast<VxUint32>(width), static_cast<VxUint32>(height),
                      RAW2RGB_NEIGHBOUR, static_cast<DX_PIXEL_COLOR_FILTER>(DeviceFormat), false);
    }

    /// Launch the Daheng camera server.
    void DahengServer::Launch()
    {
        if (LifeFlag)
        {
            Logger->RecordError("Launch() is invoked when life flag is true.");
            return;
        }

        LifeFlag = true;

        // Open camera.
        Logger->RecordMilestone("Try to open the camera " + DeviceName + "...");
        Open(DeviceIndex);
        Logger->RecordMilestone("Camera " + DeviceName + " opened.");

        Connection->sadd("cameras", DeviceName);
        Connection->sadd("cameras/" + DeviceName + "/pictures", "main");

        // Configure camera.
        Logger->RecordMilestone("Try to configure camera...");
        auto exposure = Configurator->Get<unsigned int>("Exposure");
        if (exposure) SetExposure(*exposure);
        auto gain = Configurator->Get<double>("Gain");
        if (gain) SetGain(*gain);
        auto balance_red = Configurator->Get<double>("WhiteBalanceRed");
        if (balance_red) SetWhiteBalanceRed(*balance_red);
        auto balance_green = Configurator->Get<double>("WhiteBalanceGreen");
        if (balance_green) SetWhiteBalanceGreen(*balance_green);
        auto balance_blue = Configurator->Get<double>("WhiteBalanceBlue");
        if (balance_blue) SetWhiteBalanceBlue(*balance_blue);
        Logger->RecordMilestone("Camera configured.");

        long width = 0, height = 0;
        if (GXGetInt(DeviceHandle, GX_INT_WIDTH, &width) != GX_STATUS_SUCCESS ||
            GXGetInt(DeviceHandle, GX_INT_HEIGHT, &height) != GX_STATUS_SUCCESS)
        {
            Logger->RecordError("Failed to query the width or height of the picture.");
        }
        Connection->set("cameras/" + DeviceName + "/pictures/main/width", std::to_string(width));
        Connection->set("cameras/" + DeviceName + "/pictures/main/height", std::to_string(height));
        Connection->set("cameras/" + DeviceName + "/pictures/main/channels", "3");
        Connection->set("cameras/" + DeviceName + "/pictures/main/format", "BGR");
        Logger->RecordMilestone("Picture information registered.");

        PictureSource.Create(DeviceName + ".main", width * height * 3 + 256);
        Logger->RecordMilestone("Shared memory block named '" + DeviceName + ".main created, size " +
            std::to_string(PictureSource.GetSize()));

        auto last_status_update_time = std::chrono::system_clock::now();
        while (LifeFlag)
        {
            // Time out time is 10ms.
            try
            {
                Subscriber->consume();
            }catch (sw::redis::TimeoutError& error)
            {}

            auto current_time = std::chrono::system_clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(
                    current_time - last_status_update_time).count() >= 1000)
            {
                UpdateStatus();
                last_status_update_time = current_time;
            }
        }

        Connection->srem("cameras", DeviceName);
        Connection->del("cameras/" + DeviceName + "/pictures");
        Connection->del("cameras/" + DeviceName + "/pictures/main/width");
        Connection->del("cameras/" + DeviceName + "/pictures/main/height");
        Connection->del("cameras/" + DeviceName + "/pictures/main/channels");
        Connection->del("cameras/" + DeviceName + "/pictures/main/format");
        Logger->RecordMilestone("Picture information unregistered.");

        PictureSource.Delete();
        Logger->RecordMilestone("Shared memory block removed.");

        Close();
        Logger->RecordMilestone("Camera closed.");
    }

    /// Update the camera status into Redis server.
    void DahengServer::UpdateStatus()
    {
        Connection->set("cameras/" + DeviceName + "/status/fps", std::to_string(ReceivedPicturesCount));
        ReceivedPicturesCount = 0;
    }

    /// Handle command.
    void DahengServer::HandleCommand(const std::string &command)
    {
        if (command == "shutdown")
        {
            Logger->RecordMilestone("Shutdown command received.");
            Close();
            LifeFlag = false;
        }
        else if (command == "save")
        {
            Configurator->Apply();
            Logger->RecordMessage("Configuration saved.");
        }
        else if (command == "update_exposure")
        {
            auto exposure = Configurator->Get<unsigned int>("Exposure");
            if (exposure)
            {
                if (SetExposure(*exposure))
                {
                    Logger->RecordMessage("Exposure updated to " + std::to_string(*exposure));
                }
                else
                {
                    Logger->RecordError("Try to update exposure to " + std::to_string(*exposure) + " but failed.");
                }
            }
            else
            {
                Logger->RecordWarning("Exposure is required to update, but its configuration value is missing.");
            }
        }
        else if (command == "update_gain")
        {
            auto gain = Configurator->Get<double>("Gain");
            if (gain)
            {
                if (SetGain(*gain))
                {
                    Logger->RecordMessage("Gain updated to " + std::to_string(*gain));
                }
                else
                {
                    Logger->RecordError("Try to update gain to " + std::to_string(*gain) + " but failed.");
                }
            }
            else
            {
                Logger->RecordWarning("Gain is required to update, but its configuration value is missing.");
            }
        }
        else if (command == "update_white_balance")
        {
            auto balance_red = Configurator->Get<double>("WhiteBalanceRed");
            if (balance_red)
            {
                if (SetWhiteBalanceRed(*balance_red))
                {
                    Logger->RecordMessage("White balance red channel updated to " + std::to_string(*balance_red));
                }
                else
                {
                    Logger->RecordError("Try to update white balance red channel to " +
                        std::to_string(*balance_red) + " but failed.");
                }
            }
            auto balance_green = Configurator->Get<double>("WhiteBalanceGreen");
            if (balance_green)
            {
                if (SetWhiteBalanceGreen(*balance_green))
                {
                    Logger->RecordMessage("White balance green channel updated to " + std::to_string(*balance_green));
                }
                else
                {
                    Logger->RecordError("Try to update white balance green channel to " +
                                        std::to_string(*balance_green) + " but failed.");
                }
            }
            auto balance_blue = Configurator->Get<double>("WhiteBalanceBlue");
            if (balance_blue)
            {
                if (SetWhiteBalanceBlue(*balance_blue))
                {
                    Logger->RecordMessage("White balance blue channel updated to " + std::to_string(*balance_blue));
                }
                else
                {
                    Logger->RecordError("Try to update white balance blue channel to " +
                                        std::to_string(*balance_blue) + " but failed.");
                }
            }
        }
        else
        {
            Logger->RecordWarning("Unknown command '" + command + "' received.");
        }
    }

    /// Open the given camera device.
    void DahengServer::Open(unsigned int device_index)
    {
        if (DeviceHandle)
        {
            Close();
        }

        GX_STATUS operation_result;

        uint32_t device_count = 0;
        operation_result = GXUpdateDeviceList(&device_count, 500);
        if (operation_result != GX_STATUS_LIST::GX_STATUS_SUCCESS)
        {
            Logger->RecordError("Failed to open camera: failed to query camera devices list.");
            throw std::runtime_error("Failed to open camera: driver error.");
        }
        if (device_count <= 0)
        {
            Logger->RecordError("Failed to open camera: no camera detected.");
            throw std::runtime_error("Failed to open camera: no camera detected.");
        }
        if (device_count <= device_index)
        {
            Logger->RecordError("Failed to open camera: invalid camera index " + std::to_string(device_index) +
                ", detected cameras count is " + std::to_string(device_count));
            throw std::runtime_error("Failed to open camera: invalid camera index.");
        }

        // Open the camera of the given index. Notice that cameras are indexed from 1 in Daheng SDK.
        operation_result = GXOpenDeviceByIndex(device_index + 1, &DeviceHandle);
        if (operation_result != GX_STATUS_LIST::GX_STATUS_SUCCESS)
        {
            Logger->RecordError("Failed to open camera.");
            throw std::runtime_error("Failed to open camera.");
        }

        operation_result = GXRegisterCaptureCallback(DeviceHandle,
                                                     this,
                                                     reinterpret_cast<GXCaptureCallBack>(CameraCaptureCallback));
        if (operation_result != GX_STATUS_LIST::GX_STATUS_SUCCESS)
        {
            Logger->RecordError("Failed to open camera: failed to register capture call back.");
            throw std::runtime_error("Failed to open camera: driver error.");
        }

        operation_result = GXSendCommand(DeviceHandle,
                                         GX_COMMAND_ACQUISITION_START);
        if (operation_result != GX_STATUS_LIST::GX_STATUS_SUCCESS)
        {
            Logger->RecordError("Failed to open camera: failed to start acquisition.");
            throw std::runtime_error("Failed to open camera: driver error.");
        }
    }

    /// Close the opened camera device.
    void DahengServer::Close()
    {
        if (!DeviceHandle) return;
        GXSendCommand(DeviceHandle, GX_COMMAND_ACQUISITION_STOP);
        GXUnregisterCaptureCallback(DeviceHandle);
        GXCloseDevice(DeviceHandle);
        DeviceHandle = nullptr;
    }

    /// Set the exposure of the camera.
    bool DahengServer::SetExposure(unsigned int microseconds)
    {
        if (DeviceHandle &&
            GXSetFloat(DeviceHandle, GX_FLOAT_EXPOSURE_TIME, microseconds) == GX_STATUS_LIST::GX_STATUS_SUCCESS)
        {
            return true;
        }
        return false;
    }

    /// Set the digital gain of the camera.
    bool DahengServer::SetGain(double gain)
    {
        if (DeviceHandle &&
            GXSetFloat(DeviceHandle, GX_FLOAT_GAIN, gain) == GX_STATUS_LIST::GX_STATUS_SUCCESS)
        {
            return true;
        }
        return false;
    }

    /// Set red channel value of the white balance.
    bool DahengServer::SetWhiteBalanceRed(double ratio)
    {
        if (DeviceHandle)
        {
            GX_STATUS operation_result;
            operation_result = GXSetEnum(DeviceHandle, GX_ENUM_BALANCE_RATIO_SELECTOR,
                                         GX_BALANCE_RATIO_SELECTOR_RED);
            if (operation_result != GX_STATUS_LIST::GX_STATUS_SUCCESS)
            {
                return false;
            }
            operation_result = GXSetFloat(DeviceHandle, GX_FLOAT_BALANCE_RATIO, ratio);
            if (operation_result != GX_STATUS_LIST::GX_STATUS_SUCCESS)
            {
                return false;
            }

            return true;
        }
        return false;
    }

    /// Set blue channel value of the white balance.
    bool DahengServer::SetWhiteBalanceBlue(double ratio)
    {
        if (DeviceHandle)
        {
            GX_STATUS operation_result;
            operation_result = GXSetEnum(DeviceHandle, GX_ENUM_BALANCE_RATIO_SELECTOR,
                                         GX_BALANCE_RATIO_SELECTOR_BLUE);
            if (operation_result != GX_STATUS_LIST::GX_STATUS_SUCCESS)
            {
                return false;
            }
            operation_result = GXSetFloat(DeviceHandle, GX_FLOAT_BALANCE_RATIO, ratio);
            if (operation_result != GX_STATUS_LIST::GX_STATUS_SUCCESS)
            {
                return false;
            }

            return true;
        }
        return false;
    }

    /// Set green channel value of the white balance.
    bool DahengServer::SetWhiteBalanceGreen(double ratio)
    {
        if (DeviceHandle)
        {
            GX_STATUS operation_result;
            operation_result = GXSetEnum(DeviceHandle, GX_ENUM_BALANCE_RATIO_SELECTOR,
                                         GX_BALANCE_RATIO_SELECTOR_GREEN);
            if (operation_result != GX_STATUS_LIST::GX_STATUS_SUCCESS)
            {
                return false;
            }
            operation_result = GXSetFloat(DeviceHandle, GX_FLOAT_BALANCE_RATIO, ratio);
            if (operation_result != GX_STATUS_LIST::GX_STATUS_SUCCESS)
            {
                return false;
            }

            return true;
        }
        return false;
    }
}