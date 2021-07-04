#include "HikCamera.hpp"

#include <MvCameraControl.h>

namespace Gaia::CameraService
{
    /// Callback for camera capture event.
    void CameraCaptureCallback(unsigned char* data,
                               MV_FRAME_OUT_INFO_EX* frame_information,
                               void* user_data)
    {
        auto* target = static_cast<HikCamera*>(user_data);
        if (target)
        {
            target->OnPictureCapture(data, frame_information);
        }
    }

    /// Constructor.
    HikCamera::HikCamera() :
            CameraInterface("hik")
    {}

    /// Destructor which will automatically close the device.
    HikCamera::~HikCamera()
    {
        Close();
    }

    /// Invoked when a new picture is captured by the camera.
    void HikCamera::OnPictureCapture(unsigned char *data, void* parameters_package)
    {
        ReceivedPicturesCount++;

        auto* parameters = static_cast<MV_FRAME_OUT_INFO_EX*>(parameters_package);

        MV_CC_PIXEL_CONVERT_PARAM convert_package;
        std::memset(&convert_package, 0, sizeof(MV_CC_PIXEL_CONVERT_PARAM));
        convert_package.nWidth = parameters->nWidth;
        convert_package.nHeight = parameters->nHeight;
        convert_package.pSrcData = static_cast<unsigned char *>(data);
        convert_package.nSrcDataLen = parameters->nFrameLen;
        convert_package.pDstBuffer = reinterpret_cast<unsigned char*>(PictureMemory.GetPointer());
        convert_package.nDstBufferSize = PictureMemory.GetSize();
        convert_package.enSrcPixelType = parameters->enPixelType;
        convert_package.enDstPixelType = PixelType_Gvsp_BGR8_Packed;
        if (MV_CC_ConvertPixelType(DeviceHandle, &convert_package) != MV_OK)
        {
            GetLogger()->RecordError("Failed to convert the captured picture into BGR, pixel type " +
                std::to_string(parameters->enPixelType));
        }
        UpdatePictureTimestamp("main");
    }

    /// Open the camera.
    void HikCamera::Open()
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
        PictureMemory.Create(DeviceName + ".main",
                             static_cast<long>(GetPictureWidth() * GetPictureHeight() * GetPictureChannels().size() + 250));

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
    }

    /// Close the opened camera device.
    void HikCamera::Close()
    {
        if (!DeviceHandle) return;
        MV_CC_StopGrabbing(DeviceHandle);
        MV_CC_CloseDevice(DeviceHandle);
        MV_CC_DestroyHandle(DeviceHandle);
        DeviceHandle = nullptr;

        PictureMemory.Delete();
    }

    /// Get current frames per seconds.
    unsigned int HikCamera::AcquireReceivedFrameCount()
    {
        auto count = ReceivedPicturesCount.load();
        ReceivedPicturesCount = 0;
        return count;
    }

    /// Set the exposure of the camera.
    bool HikCamera::SetExposure(unsigned int microseconds)
    {
        if (DeviceHandle)
        {
            return MV_CC_SetFloatValue(DeviceHandle, "ExposureTime", static_cast<float>(microseconds)) == MV_OK;
        }
        return false;
    }

    /// Get exposure time.
    unsigned int HikCamera::GetExposure()
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
    bool HikCamera::SetGain(double gain)
    {
        if (DeviceHandle)
        {
            return MV_CC_SetFloatValue(DeviceHandle, "Gain", static_cast<float>(gain)) == MV_OK;
        }
        return false;
    }

    /// Get digital gain.
    double HikCamera::GetGain()
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
    bool HikCamera::SetWhiteBalanceRed(double ratio)
    {
        if (DeviceHandle && MV_CC_SetEnumValue(DeviceHandle, "BalanceWhiteAuto", 0) == MV_OK
            && MV_CC_SetEnumValue(DeviceHandle, "BalanceRatioSelector", 0) == MV_OK)
        {
            return MV_CC_SetIntValue(DeviceHandle, "BalanceRatio", static_cast<int>(ratio * 1000)) == MV_OK;
        }
        return false;
    }

    /// Get white balance red channel value.
    double HikCamera::GetWhiteBalanceRed()
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
    bool HikCamera::SetWhiteBalanceBlue(double ratio)
    {
        if (DeviceHandle && MV_CC_SetEnumValue(DeviceHandle, "BalanceWhiteAuto", 0) == MV_OK
            && MV_CC_SetEnumValue(DeviceHandle, "BalanceRatioSelector", 2) == MV_OK)
        {
            return MV_CC_SetIntValue(DeviceHandle, "BalanceRatio", static_cast<int>(ratio * 1000)) == MV_OK;
        }
        return false;
    }

    /// Get white balance blue channel value.
    double HikCamera::GetWhiteBalanceBlue()
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
    bool HikCamera::SetWhiteBalanceGreen(double ratio)
    {
        if (DeviceHandle && MV_CC_SetEnumValue(DeviceHandle, "BalanceWhiteAuto", 0) == MV_OK
            && MV_CC_SetEnumValue(DeviceHandle, "BalanceRatioSelector",1) == MV_OK)
        {
            return MV_CC_SetIntValue(DeviceHandle, "BalanceRatio", static_cast<int>(ratio * 1000)) == MV_OK;
        }
        return false;
    }

    /// Get white balance green channel value.
    double HikCamera::GetWhiteBalanceGreen()
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
    bool HikCamera::AutoAdjustExposure()
    {
        if (DeviceHandle)
        {
            return MV_CC_SetEnumValue(DeviceHandle, "ExposureAuto", 1) == MV_OK;
        }
        return false;
    }

    /// Auto adjust the gain for once.
    bool HikCamera::AutoAdjustGain()
    {
        if (DeviceHandle)
        {
            return MV_CC_SetEnumValue(DeviceHandle, "GainAuto", 1) == MV_OK;
        }
        return false;
    }

    /// Auto adjust white balance for once.
    bool HikCamera::AutoAdjustWhiteBalance()
    {
        if (!DeviceHandle) return false;
        if (MV_CC_SetEnumValue(DeviceHandle, "BalanceWhiteAuto", 1) != MV_OK) return false;
        return true;
    }

    /// Get width of the picture.
    long HikCamera::GetPictureWidth()
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
    long HikCamera::GetPictureHeight()
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

    /// Get channels description.
    std::vector<std::vector<char>> HikCamera::GetPictureChannels()
    {
        return {{'B', 'G', 'R'}};
    }

    /// Get format name of the picture.
    std::vector<std::string> HikCamera::GetPictureFormats()
    {
        return {"8U"};
    }

    /// Get picture names list.
    std::vector<std::string> HikCamera::GetPictureNames()
    {
        return {"main"};
    }
}