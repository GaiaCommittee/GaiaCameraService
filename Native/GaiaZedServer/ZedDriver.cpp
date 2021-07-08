#include "ZedDriver.hpp"

#include <opencv2/opencv.hpp>

namespace Gaia::CameraService
{
    /// Convert sl::MAT_TYPE to OpenCV type.
    int ConvertToOpenCVType(sl::MAT_TYPE type)
    {
        int cv_type = -1;
        switch (type)
        {
            case sl::MAT_TYPE::F32_C1: cv_type = CV_32FC1; break;
            case sl::MAT_TYPE::F32_C2: cv_type = CV_32FC2; break;
            case sl::MAT_TYPE::F32_C3: cv_type = CV_32FC3; break;
            case sl::MAT_TYPE::F32_C4: cv_type = CV_32FC4; break;
            case sl::MAT_TYPE::U8_C1: cv_type = CV_8UC1; break;
            case sl::MAT_TYPE::U8_C2: cv_type = CV_8UC2; break;
            case sl::MAT_TYPE::U8_C3: cv_type = CV_8UC3; break;
            case sl::MAT_TYPE::U8_C4: cv_type = CV_8UC4; break;
            default: break;
        }
        return cv_type;
    }

    /// Converted cv::Mat are sharing memory with the sl::Mat.
    cv::Mat ConvertToOpenCVMat(const sl::Mat& matrix)
    {
        return cv::Mat(static_cast<int>(matrix.getHeight()), static_cast<int>(matrix.getWidth()),
                       ConvertToOpenCVType(matrix.getDataType()),
                       matrix.getPtr<sl::uchar1>(sl::MEM::CPU),
                       matrix.getStepBytes(sl::MEM::CPU));
    }

    /// Retrieve and upload a view picture from the device to the shared memory.
    void UploadZedBGRAPicture(LogService::LogClient* logger, sl::Camera& device,
                              sl::VIEW view, SharedPicture::PictureWriter& writer)
    {
        sl::Mat sl_matrix;
        device.retrieveImage(sl_matrix, view, sl::MEM::CPU);
        auto matrix = ConvertToOpenCVMat(sl_matrix);

        if (writer.GetMaxSize() < matrix.elemSize())
        {
            logger->RecordError("Insufficient memory to upload Zed picture of view ID " +
                                std::to_string(static_cast<int>(view)));
            return;
        }
        /// Function std::memcpy(...) can not function properly here.
        cv::Mat shared_picture(cv::Size(matrix.cols, matrix.rows), matrix.type(),
                               writer.GetPointer());
        matrix.copyTo(shared_picture);
    }

    /// Retrieve and update a point cloud from the device to the shared memory.
    void UploadZedPointCloud(LogService::LogClient* logger, sl::Camera& device,
                             SharedPicture::PictureWriter& writer)
    {
        sl::Mat sl_matrix;
        // Channels are X,Y,Z, BGRA (8 * 4 merged int a single 32 channel).
        device.retrieveMeasure(sl_matrix, sl::MEASURE::XYZBGRA,sl::MEM::CPU);
        auto matrix = ConvertToOpenCVMat(sl_matrix);
        if (writer.GetMaxSize() < matrix.elemSize())
        {
            logger->RecordError("Insufficient memory to upload Zed point cloud.");
            return;
        }
        cv::Mat shared_picture(cv::Size(matrix.cols, matrix.rows), matrix.type(),
                               writer.GetPointer());
        matrix.copyTo(shared_picture);
    }

    /// Convert a float3 vector into string.
    std::string ConvertFloat3ToString(const sl::float3& data)
    {
        return std::to_string(data.x) + "," + std::to_string(data.y) + "," + std::to_string(data.z);
    }

    /// Constructor.
    ZedDriver::ZedDriver() :
        CameraDriverInterface("zed"),
        GrabberThread([this](const std::atomic_bool& flag){
            while (flag)
            {
                this->UpdatePicture();
            }
        })
    {}

    /// Destructor which will automatically close the device.
    ZedDriver::~ZedDriver()
    {
        Close();
    }

    /// Grab a picture and write it into the shared memory.
    void ZedDriver::UpdatePicture()
    {
        RetrievedPicturesCount++;

        if (Device.grab() != sl::ERROR_CODE::SUCCESS)
        {
            GetLogger()->RecordError("A grab attempt is failed.");
            return;
        }

        auto left_view_task = std::async(std::launch::async, [this]{
            if (!this->LeftViewWriter) return;
            UploadZedBGRAPicture(this->GetLogger(), this->Device, sl::VIEW::LEFT, *this->LeftViewWriter);
            this->UpdatePictureTimestamp("left");
        });
        auto right_view_task = std::async(std::launch::async, [this]{
            if (!this->RightViewWriter) return;
            UploadZedBGRAPicture(this->GetLogger(), this->Device, sl::VIEW::RIGHT, *this->RightViewWriter);
            this->UpdatePictureTimestamp("right");
        });
        auto point_cloud_task = std::async(std::launch::async, [this]{
            if (!this->PointCloudWriter) return;
            UploadZedPointCloud(this->GetLogger(), this->Device, *this->PointCloudWriter);
            this->UpdatePictureTimestamp("point_cloud");
        });

        // Update sensor data.
        auto status_prefix = "cameras/" + DeviceName + "/status";
        auto* database = GetDatabase();

        sl::SensorsData sensors_data;
        Device.getSensorsData(sensors_data, sl::TIME_REFERENCE::IMAGE);

        auto magnetic_field = sensors_data.magnetometer.magnetic_field_calibrated;
        database->set(status_prefix + "/magnetic_field", ConvertFloat3ToString(magnetic_field));
        auto relative_altitude = sensors_data.barometer.pressure;
        database->set(status_prefix + "/relative_altitude", std::to_string(relative_altitude));
        auto linear_acceleration = sensors_data.imu.linear_acceleration;
        database->set(status_prefix + "/linear_acceleration", ConvertFloat3ToString(linear_acceleration));
        auto angular_velocity = sensors_data.imu.angular_velocity;
        database->set(status_prefix + "/angular_velocity", ConvertFloat3ToString(angular_velocity));
        auto pose = sensors_data.imu.pose.getRotationVector();
        database->set(status_prefix + "/orientation", ConvertFloat3ToString(pose));

        // Block this thread until those tasks are done.
        left_view_task.get();
        right_view_task.get();
        point_cloud_task.get();

        LastReceiveTimePoint = std::chrono::steady_clock::now();
    }

    /// Open the camera.
    void ZedDriver::Open()
    {
        sl::InitParameters parameters;
        auto option_resolution = GetConfigurator()->Get("Resolution").value_or("HD720");
        if (option_resolution == "HD720"){
            parameters.camera_resolution = sl::RESOLUTION::HD720;
        }else if (option_resolution == "HD1080"){
            parameters.camera_resolution = sl::RESOLUTION::HD1080;
        }else if (option_resolution == "VGA"){
            parameters.camera_resolution = sl::RESOLUTION::VGA;
        }else if (option_resolution == "HD2K"){
            parameters.camera_resolution = sl::RESOLUTION::HD2K;
        }else{
            parameters.camera_resolution = sl::RESOLUTION::LAST;
        }
        parameters.camera_fps = GetConfigurator()->Get<int>("FPS").value_or(60);
        parameters.coordinate_units = sl::UNIT::CENTIMETER;
        auto option_depth_mode = GetConfigurator()->Get("DepthMode").value_or("Performance");
        if (option_depth_mode == "Performance"){
            parameters.depth_mode = sl::DEPTH_MODE::PERFORMANCE;
        }else if (option_depth_mode == "Quality"){
            parameters.depth_mode = sl::DEPTH_MODE::QUALITY;
        }else if (option_depth_mode == "Ultra"){
            parameters.depth_mode = sl::DEPTH_MODE::ULTRA;
        }
        auto optional_max_distance = GetConfigurator()->Get("MaxDepth");
        if (optional_max_distance)
        {
            parameters.depth_maximum_distance = std::stof(*optional_max_distance);
        }
        auto optional_min_distance = GetConfigurator()->Get("MinDepth");
        if (optional_min_distance)
        {
            parameters.depth_minimum_distance = std::stof(*optional_min_distance);
        }
        auto operation_result = Device.open(parameters);
        if (operation_result != sl::ERROR_CODE::SUCCESS)
        {
            GetLogger()->RecordError("Failed to open Zed camera, error code:" +
                std::to_string(static_cast<double>(operation_result)));
            throw std::runtime_error("Failed to open Zed camera, error code:" +
                                     std::to_string(static_cast<double>(operation_result)));
        }

        auto picture_resolution = Device.getCameraInformation().camera_configuration.resolution;
        auto picture_size = picture_resolution.width * picture_resolution.height;

        SharedPicture::PictureHeader picture_header;
        picture_header.PixelType = SharedPicture::PictureHeader::PixelTypes::Unsigned;
        picture_header.PixelBits = SharedPicture::PictureHeader::PixelBitSizes::Bits8;
        picture_header.Channels = 4;
        picture_header.Width = picture_resolution.width;
        picture_header.Height = picture_resolution.height;

        LeftViewWriter = std::make_unique<SharedPicture::PictureWriter>(
                DeviceName + ".left",static_cast<long>(picture_size * 4), true);
        LeftViewWriter->SetHeader(picture_header);
        RightViewWriter = std::make_unique<SharedPicture::PictureWriter>(
                DeviceName + ".right",static_cast<long>(picture_size * 4), true);
        RightViewWriter->SetHeader(picture_header);

        SharedPicture::PictureHeader point_cloud_header;
        point_cloud_header.PixelType = SharedPicture::PictureHeader::PixelTypes::Float;
        point_cloud_header.PixelBits = SharedPicture::PictureHeader::PixelBitSizes::Bits32;
        point_cloud_header.Channels = 4;
        point_cloud_header.Width = picture_resolution.width;
        point_cloud_header.Height = picture_resolution.height;

        // 1 float equals 4 chars in size.
        PointCloudWriter = std::make_unique<SharedPicture::PictureWriter>(
                DeviceName + ".point_cloud",
                static_cast<long>(picture_size * 4 * 4), true);
        PointCloudWriter->SetHeader(point_cloud_header);

        LastReceiveTimePoint = std::chrono::steady_clock::now();

        GrabberThread.Start();
    }

    /// Close the opened camera device.
    void ZedDriver::Close()
    {
        GrabberThread.Stop();
        if (Device.isOpened())
        {
            Device.close();
        }
        if (LeftViewWriter) LeftViewWriter->Release();
        if (RightViewWriter) RightViewWriter->Release();
        if (PointCloudWriter) PointCloudWriter->Release();
    };

    /// Check whether this camera is alive or not.
    bool ZedDriver::IsAlive()
    {
        if (std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - LastReceiveTimePoint.load()).count() > 1)
        {
            return false;
        }
        return true;
    }

    /// Set the exposure of the camera.
    bool ZedDriver::SetExposure(unsigned int percentage)
    {
        if (!Device.isOpened()) return false;
        Device.setCameraSettings(sl::VIDEO_SETTINGS::EXPOSURE, static_cast<int>(percentage));
        return true;
    }

    /// Get exposure time.
    unsigned int ZedDriver::GetExposure()
    {
        if (!Device.isOpened()) return 0;
        return static_cast<unsigned int>(Device.getCameraSettings(sl::VIDEO_SETTINGS::EXPOSURE));
    }

    /// Set the digital gain of the camera.
    bool ZedDriver::SetGain(double gain)
    {
        if (!Device.isOpened()) return false;
        Device.setCameraSettings(sl::VIDEO_SETTINGS::GAIN, static_cast<int>(gain));
        return true;
    }

    /// Get digital gain.
    double ZedDriver::GetGain()
    {
        if (!Device.isOpened()) return 0.0f;
        return static_cast<double>(Device.getCameraSettings(sl::VIDEO_SETTINGS::GAIN));
    }

    /// Auto adjust the exposure for once.
    bool ZedDriver::AutoAdjustExposure()
    {
        if (!Device.isOpened()) return false;
        Device.setCameraSettings(sl::VIDEO_SETTINGS::EXPOSURE,
                                 sl::VIDEO_SETTINGS_VALUE_AUTO);
        return true;
    }

    /// Auto adjust the gain for once.
    bool ZedDriver::AutoAdjustGain()
    {
        if (!Device.isOpened()) return false;
        Device.setCameraSettings(sl::VIDEO_SETTINGS::GAIN,
                                 sl::VIDEO_SETTINGS_VALUE_AUTO);
        return true;
    }

    /// Auto adjust white balance for once.
    bool ZedDriver::AutoAdjustWhiteBalance()
    {
        if (!Device.isOpened()) return false;
        Device.setCameraSettings(sl::VIDEO_SETTINGS::WHITEBALANCE_TEMPERATURE,
                                        sl::VIDEO_SETTINGS_VALUE_AUTO);
        return true;
    }

    /// Get width of the picture.
    long ZedDriver::GetPictureWidth()
    {
        if (Device.isOpened())
        {
            return static_cast<long>(Device.getCameraInformation().camera_configuration.resolution.width);
        }
        else
        {
            GetLogger()->RecordError("Get picture width before open the camera.");
            return 0;
        }
    }

    /// Get height of the picture.
    long ZedDriver::GetPictureHeight()
    {
        if (Device.isOpened())
        {
            return static_cast<long>(Device.getCameraInformation().camera_configuration.resolution.height);
        }
        else
        {
            GetLogger()->RecordError("Get picture width before open the camera.");
            return 0;
        }
    }

    /// Get picture names list.
    std::vector<std::tuple<std::string, std::string>> ZedDriver::GetPictureNames()
    {
        return {{"left", "BGR"}, {"right", "BGR"}, {"point_cloud", "XYZC"}};
    }


    /// Set red channel value of the white balance.
    bool ZedDriver::SetWhiteBalanceRed(double ratio)
    {
        GetLogger()->RecordWarning("White balance red channel is required to set to " + std::to_string(ratio) +
            ", but this function is not supported yet on ZED camera.");
        return false;
    }

    /// Get red channel value of the white balance.
    double ZedDriver::GetWhiteBalanceRed()
    {
        GetLogger()->RecordWarning("White balance red channel is required, "
                                   "but this function is not supported yet.");
        return 0.0f;
    }

    /// Set blue channel value of the white balance.
    bool ZedDriver::SetWhiteBalanceBlue(double ratio)
    {
        GetLogger()->RecordWarning("White balance red channel is required to set to " + std::to_string(ratio) +
                                   ", but this function is not supported yet on ZED camera.");
        return false;
    }

    /// Get blue channel value of the white balance.
    double ZedDriver::GetWhiteBalanceBlue()
    {
        GetLogger()->RecordWarning("White balance blue channel is required, "
                               "but this function is not supported yet.");
        return 0.0f;
    }

    /// Set green channel value of the white balance.
    bool ZedDriver::SetWhiteBalanceGreen(double ratio)
    {
        GetLogger()->RecordWarning("White balance red channel is required to set to " + std::to_string(ratio) +
                                   ", but this function is not supported yet on ZED camera.");
        return false;
    }

    /// Get green channel value of the white balance.
    double ZedDriver::GetWhiteBalanceGreen()
    {
        GetLogger()->RecordWarning("White balance green channel is required, "
                                   "but this function is not supported yet.");
        return 0.0f;
    }
}