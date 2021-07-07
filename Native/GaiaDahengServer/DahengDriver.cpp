#include "DahengDriver.hpp"

#include <GxIAPI.h>
#include <DxImageProc.h>

namespace Gaia::CameraService
{
    /// Callback for camera capture event.
    void CameraCaptureCallback(GX_FRAME_CALLBACK_PARAM* parameters_package)
    {
        auto* target = static_cast<DahengDriver*>(parameters_package->pUserParam);
        if (target)
        {
            target->OnPictureCapture(parameters_package);
        }
    }

    /// Constructor.
    DahengDriver::DahengDriver() :
        CameraDriverInterface("daheng")
    {}

    /// Destructor which will automatically close the device.
    DahengDriver::~DahengDriver()
    {
        Close();
    }

    /// Invoked when a new picture is captured by the camera.
    void DahengDriver::OnPictureCapture(void *parameters_package)
    {
        auto* parameters = static_cast<GX_FRAME_CALLBACK_PARAM*>(parameters_package);

        auto pixel_type = static_cast<GX_PIXEL_FORMAT_ENTRY>(parameters->nPixelFormat);
        DX_PIXEL_COLOR_FILTER converter_id = BAYERRG;
        if (pixel_type == GX_PIXEL_FORMAT_ENTRY::GX_PIXEL_FORMAT_BAYER_RG8)
        {
            converter_id = DX_PIXEL_COLOR_FILTER::BAYERBG;
        } else if (pixel_type == GX_PIXEL_FORMAT_ENTRY::GX_PIXEL_FORMAT_BAYER_GR8)
        {
            converter_id = DX_PIXEL_COLOR_FILTER::BAYERGB;
        } else if (pixel_type == GX_PIXEL_FORMAT_ENTRY::GX_PIXEL_FORMAT_BAYER_BG8)
        {
            converter_id = DX_PIXEL_COLOR_FILTER::BAYERRG;
        } else if (pixel_type == GX_PIXEL_FORMAT_ENTRY::GX_PIXEL_FORMAT_BAYER_GB8)
        {
            converter_id = DX_PIXEL_COLOR_FILTER::BAYERGR;
        }

        ReceivedPicturesCount++;

        auto status = DxRaw8toRGB24(const_cast<void*>(parameters->pImgBuf), Writer->GetPointer(),
                      static_cast<VxUint32>(parameters->nWidth), static_cast<VxUint32>(parameters->nHeight),
                      RAW2RGB_NEIGHBOUR, converter_id, false);
        if (status != DX_STATUS::DX_OK)
        {
            GetLogger()->RecordError("Failed to convert captured picture to BGR, pixel type "
                + std::to_string(pixel_type) + " , converter index " + std::to_string(converter_id));
        }
        UpdatePictureTimestamp("main");

        LastReceiveTimePoint = std::chrono::steady_clock::now();
    }

    /// Open the camera.
    void DahengDriver::Open()
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
            GetLogger()->RecordError("Failed to open camera: failed to query camera devices list.");
            throw std::runtime_error("Failed to open camera: failed to query camera devices list.");
        }
        if (device_count <= 0)
        {
            GetLogger()->RecordError("Failed to open camera: no camera detected.");
            throw std::runtime_error("Failed to open camera: no camera detected.");
        }
        if (device_count <= DeviceIndex)
        {
            GetLogger()->RecordError("Failed to open camera: invalid camera index " + std::to_string(DeviceIndex) +
                                     ", detected cameras count is " + std::to_string(device_count));
            throw std::runtime_error("Failed to open camera: invalid camera index " + std::to_string(DeviceIndex) +
                                     ", detected cameras count is " + std::to_string(device_count));
        }

        // Open the camera of the given index. Notice that cameras are indexed from 1 in Daheng SDK.
        operation_result = GXOpenDeviceByIndex(DeviceIndex + 1, &DeviceHandle);
        if (operation_result != GX_STATUS_LIST::GX_STATUS_SUCCESS)
        {
            GetLogger()->RecordError("Failed to open camera.");
            throw std::runtime_error("Failed to open camera.");
        }

        operation_result = GXRegisterCaptureCallback(DeviceHandle,
                                                     this,
                                                     reinterpret_cast<GXCaptureCallBack>(CameraCaptureCallback));
        if (operation_result != GX_STATUS_LIST::GX_STATUS_SUCCESS)
        {
            GetLogger()->RecordError("Failed to open camera: failed to register capture call back.");
            throw std::runtime_error("Failed to open camera: failed to register capture call back.");
        }

        // Prepare shared memory.
        Writer = std::make_unique<SharedPicture::PictureWriter>(DeviceName + ".main",
         static_cast<long>(GetPictureWidth() * GetPictureHeight() * 3), true);
        SharedPicture::PictureHeader picture_header;
        picture_header.PixelType = SharedPicture::PictureHeader::PixelTypes::Unsigned;
        picture_header.PixelBits = SharedPicture::PictureHeader::PixelBitSizes::Bits8;
        picture_header.Channels = 3;
        picture_header.Width = GetPictureWidth();
        picture_header.Height = GetPictureHeight();
        Writer->SetHeader(picture_header);

        // Configure acquisition rate if given.
        auto option_fps = GetConfigurator()->Get("FPS");
        if (option_fps && GXSetEnum(DeviceHandle,
                                    GX_ENUM_ACQUISITION_FRAME_RATE_MODE,
                                    GX_ACQUISITION_FRAME_RATE_MODE_ENTRY::GX_ACQUISITION_FRAME_RATE_MODE_ON) ==
                          GX_STATUS_LIST::GX_STATUS_SUCCESS)
        {
            float acquisition_rate = std::stof(*option_fps);
            if (GXSetFloat(DeviceHandle, GX_FLOAT_ACQUISITION_FRAME_RATE, acquisition_rate) ==
                GX_STATUS_LIST::GX_STATUS_SUCCESS)
            {
                GetLogger()->RecordMessage("Switch on acquisition rate control mode,"
                                           " target rate:" + *option_fps);
            }
        }
        else
        {
            GXSetEnum(DeviceHandle,
                      GX_ENUM_ACQUISITION_FRAME_RATE_MODE, GX_ACQUISITION_FRAME_RATE_MODE_ENTRY::GX_ACQUISITION_FRAME_RATE_MODE_ON);
        }

        operation_result = GXSendCommand(DeviceHandle,
                                         GX_COMMAND_ACQUISITION_START);
        if (operation_result != GX_STATUS_LIST::GX_STATUS_SUCCESS)
        {
            GetLogger()->RecordError("Failed to open camera: failed to start acquisition.");
            throw std::runtime_error("Failed to open camera: failed to start acquisition.");
        }

        LastReceiveTimePoint = std::chrono::steady_clock::now();
    }

    /// Close the opened camera device.
    void DahengDriver::Close()
    {
        if (!DeviceHandle) return;
        GXSendCommand(DeviceHandle, GX_COMMAND_ACQUISITION_STOP);
        GXUnregisterCaptureCallback(DeviceHandle);
        GXCloseDevice(DeviceHandle);
        DeviceHandle = nullptr;

        if (Writer) Writer->Release();
    }

    /// Check timestamp.
    bool DahengDriver::IsAlive()
    {
        if (std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - LastReceiveTimePoint.load()).count() > 1)
        {
            return false;
        }
        return true;
    }

    /// Set the exposure of the camera.
    bool DahengDriver::SetExposure(unsigned int microseconds)
    {
        if (DeviceHandle &&
            GXSetFloat(DeviceHandle, GX_FLOAT_EXPOSURE_TIME, microseconds) == GX_STATUS_LIST::GX_STATUS_SUCCESS)
        {
            return true;
        }
        return false;
    }

    /// Get exposure time.
    unsigned int DahengDriver::GetExposure()
    {
        double exposure_time = 0.0;
        if (DeviceHandle)
        {
            GXGetFloat(DeviceHandle, GX_FLOAT_EXPOSURE_TIME, &exposure_time);
        }
        return static_cast<unsigned int>(exposure_time);
    }

    /// Set the digital gain of the camera.
    bool DahengDriver::SetGain(double gain)
    {
        if (DeviceHandle &&
            GXSetFloat(DeviceHandle, GX_FLOAT_GAIN, gain) == GX_STATUS_LIST::GX_STATUS_SUCCESS)
        {
            return true;
        }
        return false;
    }

    /// Get digital gain.
    double DahengDriver::GetGain()
    {
        double gain = 0.0;
        if (DeviceHandle)
        {
            GXGetFloat(DeviceHandle, GX_FLOAT_GAIN, &gain);
        }
        return gain;
    }

    /// Set red channel value of the white balance.
    bool DahengDriver::SetWhiteBalanceRed(double ratio)
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

    /// Get white balance red channel value.
    double DahengDriver::GetWhiteBalanceRed()
    {
        double value = 0.0;
        if (DeviceHandle && GXSetEnum(DeviceHandle,
                                      GX_ENUM_BALANCE_RATIO_SELECTOR,
                                      GX_BALANCE_RATIO_SELECTOR_RED)
                            == GX_STATUS_LIST::GX_STATUS_SUCCESS)
        {
            GXGetFloat(DeviceHandle, GX_FLOAT_BALANCE_RATIO, &value);
        }
        return value;
    }

    /// Set blue channel value of the white balance.
    bool DahengDriver::SetWhiteBalanceBlue(double ratio)
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

    /// Get white balance blue channel value.
    double DahengDriver::GetWhiteBalanceBlue()
    {
        double value = 0.0;
        if (DeviceHandle && GXSetEnum(DeviceHandle,
                                      GX_ENUM_BALANCE_RATIO_SELECTOR,
                                      GX_BALANCE_RATIO_SELECTOR_BLUE)
                            == GX_STATUS_LIST::GX_STATUS_SUCCESS)
        {
            GXGetFloat(DeviceHandle, GX_FLOAT_BALANCE_RATIO, &value);
        }
        return value;
    }

    /// Set green channel value of the white balance.
    bool DahengDriver::SetWhiteBalanceGreen(double ratio)
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

    /// Get white balance green channel value.
    double DahengDriver::GetWhiteBalanceGreen()
    {
        double value = 0.0;
        if (DeviceHandle && GXSetEnum(DeviceHandle,
                                      GX_ENUM_BALANCE_RATIO_SELECTOR,
                                      GX_BALANCE_RATIO_SELECTOR_GREEN)
                            == GX_STATUS_LIST::GX_STATUS_SUCCESS)
        {
            GXGetFloat(DeviceHandle, GX_FLOAT_BALANCE_RATIO, &value);
        }
        return value;
    }

    /// Auto adjust the exposure for once.
    bool DahengDriver::AutoAdjustExposure()
    {
        if (!DeviceHandle) return false;
        return GXSetEnum(DeviceHandle, GX_ENUM_EXPOSURE_AUTO, GX_EXPOSURE_AUTO_ONCE)
               == GX_STATUS_LIST::GX_STATUS_SUCCESS;
    }

    /// Auto adjust the gain for once.
    bool DahengDriver::AutoAdjustGain()
    {
        if (!DeviceHandle) return false;
        if (GXSetEnum(DeviceHandle, GX_ENUM_GAIN_SELECTOR, GX_GAIN_SELECTOR_ALL) ==
            GX_STATUS_LIST::GX_STATUS_SUCCESS)
        {
            return GXSetEnum(DeviceHandle, GX_ENUM_GAIN_AUTO, GX_GAIN_AUTO_ONCE) ==
                   GX_STATUS_LIST::GX_STATUS_SUCCESS;
        }
        return false;
    }

    /// Auto adjust white balance for once.
    bool DahengDriver::AutoAdjustWhiteBalance()
    {
        if (!DeviceHandle) return false;
        return GXSetEnum(DeviceHandle, GX_ENUM_BALANCE_WHITE_AUTO, GX_BALANCE_WHITE_AUTO_ONCE)
               == GX_STATUS_LIST::GX_STATUS_SUCCESS;
    }

    /// Get width of the picture.
    long DahengDriver::GetPictureWidth()
    {
        long width = 0;
        if (DeviceHandle)
        {
            GXGetInt(DeviceHandle, GX_INT_WIDTH, &width);
        }
        else GetLogger()->RecordError("Get width before open camera.");
        return width;
    }

    /// Get height of the picture.
    long DahengDriver::GetPictureHeight()
    {
        long height = 0;
        if (DeviceHandle)
        {
            GXGetInt(DeviceHandle, GX_INT_HEIGHT, &height);
        }else GetLogger()->RecordError("Get height before open camera.");
        return height;
    }

    /// Get picture names list.
    std::vector<std::tuple<std::string, std::string>> DahengDriver::GetPictureNames()
    {
        return {{"main", "BGR"}};
    }
}


