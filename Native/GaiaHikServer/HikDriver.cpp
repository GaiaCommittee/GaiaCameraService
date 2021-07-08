#include "HikDriver.hpp"

#include <MvCameraControl.h>

namespace Gaia::CameraService
{
    /// Callback for camera capture event.
    void CameraCaptureCallback(unsigned char* data,
                               MV_FRAME_OUT_INFO_EX* frame_information,
                               void* user_data)
    {
        auto* target = static_cast<HikDriver*>(user_data);
        if (target)
        {
            target->OnPictureCapture(data, frame_information);
        }
    }

    /// Constructor.
    HikDriver::HikDriver() :
            CameraDriverInterface("hik")
    {}

    /// Destructor which will automatically close the device.
    HikDriver::~HikDriver()
    {
        Close();
    }

    /// Invoked when a new picture is captured by the camera.
    void HikDriver::OnPictureCapture(unsigned char *data, void* parameters_package)
    {
        RetrievedPicturesCount++;

        auto* parameters = static_cast<MV_FRAME_OUT_INFO_EX*>(parameters_package);

        MV_CC_PIXEL_CONVERT_PARAM convert_package;
        std::memset(&convert_package, 0, sizeof(MV_CC_PIXEL_CONVERT_PARAM));
        convert_package.nWidth = parameters->nWidth;
        convert_package.nHeight = parameters->nHeight;
        convert_package.pSrcData = static_cast<unsigned char *>(data);
        convert_package.nSrcDataLen = parameters->nFrameLen;
        convert_package.pDstBuffer = reinterpret_cast<unsigned char*>(Writer->GetPointer());
        convert_package.nDstBufferSize = Writer->GetMaxSize();
        convert_package.enSrcPixelType = parameters->enPixelType;
        convert_package.enDstPixelType = PixelType_Gvsp_BGR8_Packed;
        if (MV_CC_ConvertPixelType(DeviceHandle, &convert_package) != MV_OK)
        {
            GetLogger()->RecordError("Failed to convert the captured picture into BGR, pixel type " +
                std::to_string(parameters->enPixelType));
        }
        UpdatePictureTimestamp("main");

        LastReceiveTimePoint = std::chrono::steady_clock::now();
    }

    /// Open the camera.
    void HikDriver::Open()
    {
        int operation_result = MV_OK;
        MV_CC_DEVICE_INFO_LIST device_info_list;
        std::memset(&device_info_list, 0, sizeof(MV_CC_DEVICE_INFO_LIST));
        operation_result = MV_CC_EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE, &device_info_list);
        if (operation_result != MV_OK)
        {
            GetLogger()->RecordError("Failed to query device list.");
            throw std::runtime_error("Failed to query device list.");
        }
        if (device_info_list.nDeviceNum <= 0)
        {
            GetLogger()->RecordError("No cameras detected.");
            throw std::runtime_error("No cameras detected.");
        }
        if (device_info_list.nDeviceNum <= DeviceIndex)
        {
            std::stringstream message;
            message << "Invalid device index: " << DeviceIndex
                    << " , camera counts: " << device_info_list.nDeviceNum;
            GetLogger()->RecordError(message.str());
            throw std::runtime_error(message.str());
        }
        operation_result = MV_CC_CreateHandle(&DeviceHandle, device_info_list.pDeviceInfo[DeviceIndex]);
        if (operation_result != MV_OK)
        {
            GetLogger()->RecordError("Failed to create device " + std::to_string(DeviceIndex) + " handle.");
            throw std::runtime_error("Failed to create device " + std::to_string(DeviceIndex) + " handle.");
        }
        operation_result = MV_CC_OpenDevice(DeviceHandle);
        if (operation_result != MV_OK)
        {
            GetLogger()->RecordError("Failed to create device " + std::to_string(DeviceIndex) + " handle.");
            throw std::runtime_error("Failed to create device " + std::to_string(DeviceIndex) + " handle.");
        }
        operation_result = MV_CC_RegisterImageCallBackEx(DeviceHandle, &CameraCaptureCallback, this);
        if (operation_result != MV_OK)
        {
            GetLogger()->RecordError("Failed to register capture callback.");
            throw std::runtime_error("Failed to register capture callback.");
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

        // Configure acquisition frames if given.
        auto option_fps = GetConfigurator()->Get("FPS");
        if (option_fps && MV_CC_SetBoolValue(DeviceHandle, "AcquisitionFrameRateEnable", true))
        {
            float acquisition_rate = std::stof(*option_fps);
            if (MV_CC_SetFloatValue(DeviceHandle, "AcquisitionFrameRate", acquisition_rate) == MV_OK)
            {
                GetLogger()->RecordMessage("Switch on acquisition rate control mode,"
                                           " target rate:" + *option_fps);
            }
        }
        else
        {
            MV_CC_SetBoolValue(DeviceHandle, "AcquisitionFrameRateEnable", false);
        }

        operation_result = MV_CC_StartGrabbing(DeviceHandle);
        if (operation_result != MV_OK)
        {
            GetLogger()->RecordError("Failed to start acquisition.");
            throw std::runtime_error("Failed to start acquisition.");
        }

        LastReceiveTimePoint = std::chrono::steady_clock::now();
    }

    /// Close the opened camera device.
    void HikDriver::Close()
    {
        if (!DeviceHandle) return;
        MV_CC_StopGrabbing(DeviceHandle);
        MV_CC_CloseDevice(DeviceHandle);
        MV_CC_DestroyHandle(DeviceHandle);
        DeviceHandle = nullptr;

        if (Writer) Writer->Release();
    }

    /// Check time point to judge whether this camera is alive or not.
    bool HikDriver::IsAlive()
    {
        if (std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - LastReceiveTimePoint.load()).count() > 1)
        {
            return false;
        }
        return true;
    }

    /// Set the exposure of the camera.
    bool HikDriver::SetExposure(unsigned int microseconds)
    {
        if (DeviceHandle)
        {
            return MV_CC_SetFloatValue(DeviceHandle, "ExposureTime", static_cast<float>(microseconds)) == MV_OK;
        }
        return false;
    }

    /// Get exposure time.
    unsigned int HikDriver::GetExposure()
    {
        double exposure_time = 0.0;
        if (DeviceHandle)
        {
            MVCC_FLOATVALUE value;
            MV_CC_GetFloatValue(DeviceHandle, "ExposureTime", &value);
            exposure_time = value.fCurValue;
        }
        return static_cast<unsigned int>(exposure_time);
    }

    /// Set the digital gain of the camera.
    bool HikDriver::SetGain(double gain)
    {
        if (DeviceHandle)
        {
            return MV_CC_SetFloatValue(DeviceHandle, "Gain", static_cast<float>(gain)) == MV_OK;
        }
        return false;
    }

    /// Get digital gain.
    double HikDriver::GetGain()
    {
        double gain = 0.0;
        if (DeviceHandle)
        {
            MVCC_FLOATVALUE value;
            MV_CC_GetFloatValue(DeviceHandle, "Gain", &value);
            return value.fCurValue;
        }
        return gain;
    }

    /// Set red channel value of the white balance.
    bool HikDriver::SetWhiteBalanceRed(double ratio)
    {
        if (DeviceHandle && MV_CC_SetEnumValue(DeviceHandle, "BalanceWhiteAuto", 0) == MV_OK
            && MV_CC_SetEnumValue(DeviceHandle, "BalanceRatioSelector", 0) == MV_OK)
        {
            return MV_CC_SetIntValue(DeviceHandle, "BalanceRatio", static_cast<int>(ratio * 1000)) == MV_OK;
        }
        return false;
    }

    /// Get white balance red channel value.
    double HikDriver::GetWhiteBalanceRed()
    {
        if (DeviceHandle && MV_CC_SetEnumValue(DeviceHandle, "BalanceWhiteAuto", 0) == MV_OK
            && MV_CC_SetEnumValue(DeviceHandle, "BalanceRatioSelector",0) == MV_OK)
        {
            MVCC_INTVALUE value;
            MV_CC_GetIntValue(DeviceHandle, "BalanceRatio", &value);
            return static_cast<double>(value.nCurValue) / 1000.0f;
        }
        return 0.0;
    }

    /// Set blue channel value of the white balance.
    bool HikDriver::SetWhiteBalanceBlue(double ratio)
    {
        if (DeviceHandle && MV_CC_SetEnumValue(DeviceHandle, "BalanceWhiteAuto", 0) == MV_OK
            && MV_CC_SetEnumValue(DeviceHandle, "BalanceRatioSelector", 2) == MV_OK)
        {
            return MV_CC_SetIntValue(DeviceHandle, "BalanceRatio", static_cast<int>(ratio * 1000)) == MV_OK;
        }
        return false;
    }

    /// Get white balance blue channel value.
    double HikDriver::GetWhiteBalanceBlue()
    {
        if (DeviceHandle && MV_CC_SetEnumValue(DeviceHandle, "BalanceWhiteAuto", 0) == MV_OK
            && MV_CC_SetEnumValue(DeviceHandle, "BalanceRatioSelector",2) == MV_OK)
        {
            MVCC_INTVALUE value;
            MV_CC_GetIntValue(DeviceHandle, "BalanceRatio", &value);
            return static_cast<double>(value.nCurValue) / 1000.0f;
        }
        return 0.0;
    }

    /// Set green channel value of the white balance.
    bool HikDriver::SetWhiteBalanceGreen(double ratio)
    {
        if (DeviceHandle && MV_CC_SetEnumValue(DeviceHandle, "BalanceWhiteAuto", 0) == MV_OK
            && MV_CC_SetEnumValue(DeviceHandle, "BalanceRatioSelector",1) == MV_OK)
        {
            return MV_CC_SetIntValue(DeviceHandle, "BalanceRatio", static_cast<int>(ratio * 1000)) == MV_OK;
        }
        return false;
    }

    /// Get white balance green channel value.
    double HikDriver::GetWhiteBalanceGreen()
    {
        if (DeviceHandle && MV_CC_SetEnumValue(DeviceHandle, "BalanceWhiteAuto", 0) == MV_OK
            && MV_CC_SetEnumValue(DeviceHandle, "BalanceRatioSelector",1) == MV_OK)
        {
            MVCC_INTVALUE value;
            MV_CC_GetIntValue(DeviceHandle, "BalanceRatio", &value);
            return static_cast<double>(value.nCurValue) / 1000.0f;
        }
        return 0.0;
    }

    /// Auto adjust the exposure for once.
    bool HikDriver::AutoAdjustExposure()
    {
        if (DeviceHandle)
        {
            return MV_CC_SetEnumValue(DeviceHandle, "ExposureAuto", 1) == MV_OK;
        }
        return false;
    }

    /// Auto adjust the gain for once.
    bool HikDriver::AutoAdjustGain()
    {
        if (DeviceHandle)
        {
            return MV_CC_SetEnumValue(DeviceHandle, "GainAuto", 1) == MV_OK;
        }
        return false;
    }

    /// Auto adjust white balance for once.
    bool HikDriver::AutoAdjustWhiteBalance()
    {
        if (!DeviceHandle) return false;
        if (MV_CC_SetEnumValue(DeviceHandle, "BalanceWhiteAuto", 1) != MV_OK) return false;
        return true;
    }

    /// Get width of the picture.
    long HikDriver::GetPictureWidth()
    {
        long width = 0;
        if (DeviceHandle)
        {
            MVCC_INTVALUE value;
            MV_CC_GetIntValue(DeviceHandle, "Width", &value);
            width = value.nCurValue;
        }
        return width;
    }

    /// Get height of the picture.
    long HikDriver::GetPictureHeight()
    {
        long height = 0;
        if (DeviceHandle)
        {
            MVCC_INTVALUE value;
            MV_CC_GetIntValue(DeviceHandle, "Height", &value);
            height = value.nCurValue;
        }
        return height;
    }

    /// Get picture names list.
    std::vector<std::tuple<std::string, std::string>> HikDriver::GetPictureNames()
    {
        return {{"main", "BGR"}};
    }
}