#pragma once

#include <memory>
#include <GaiaSharedPicture/GaiaSharedPicture.hpp>
#include <GaiaCameraServer/GaiaCameraServer.hpp>
#include <GaiaBackground/GaiaBackground.hpp>
#include <sl/Camera.hpp>

namespace Gaia::CameraService
{
    class ZedDriver : public CameraDriverInterface
    {
    private:
        /// Zed camera device.
        sl::Camera Device;
        /// Count of pictures captured by the camera.
        std::atomic<unsigned int> ReceivedPicturesCount{0};
        /// Shared memory for the captured left view picture.
        std::unique_ptr<SharedPicture::PictureWriter> LeftViewWriter;
        /// Shared memory for the captured right view picture.
        std::unique_ptr<SharedPicture::PictureWriter> RightViewWriter;
        /// Shared memory for the point cloud picture.
        std::unique_ptr<SharedPicture::PictureWriter> PointCloudWriter;
        /// Background acquisition thread.
        Background::BackgroundWorker GrabberThread;

        /// Grab a picture and write it into the shared memory.
        void UpdatePicture();

    public:
        /// Default constructor.
        ZedDriver();

        /// Auto close the camera.
        ~ZedDriver() override;

        /**
         * @brief Get width of the picture.
         * @pre Camera is opened.
         */
        long GetPictureWidth();

        /**
         * @brief Get height of the picture.
         * @pre Camera is opened.
         */
        long GetPictureHeight();

        /// Get picture names.
        std::vector<std::tuple<std::string, std::string>> GetPictureNames() override;

        /// Open the camera on the bound index and start acquisition.
        void Open() override;

        /// Close the camera and stop acquisition.
        void Close() override;

        /**
         * @brief Get the exposure of the camera.
         * @return Microseconds of the exposure time.
         */
        bool SetExposure(unsigned int percentage) override;

        /**
         * @brief Get the exposure of the camera.
         * @return Microseconds of the exposure time.
         */
        unsigned int GetExposure() override;

        /**
         * @brief Set the digital gain of the camera.
         * @param gain Value of the digital gain.
         */
        bool SetGain(double gain) override;

        /// Get the value of the digital gain of the camera.
        double GetGain() override;

        /**
         * @brief Set red channel value of the white balance.
         * @param ratio Value of the target channel.
         */
        bool SetWhiteBalanceRed(double ratio) override;

        /// Get red channel value of the white balance.
        double GetWhiteBalanceRed() override;

        /**
         * @brief Set blue channel value of the white balance.
         * @param ratio Value of the target channel.
         * @warning This function is not supported, a warning will be recorded.
         */
        bool SetWhiteBalanceBlue(double ratio) override;

        /**
         * @brief Get blue channel value of the white balance.
         * @warning This function is not supported, a warning will be recorded.
         */
        double GetWhiteBalanceBlue() override;

        /**
         * @brief Set green channel value of the white balance.
         * @param ratio Value of the target channel.
         * @warning This function is not supported, a warning will be recorded.
         */
        bool SetWhiteBalanceGreen(double ratio) override;

        /**
         * @brief Get green channel value of the white balance.
         * @warning This function is not supported, a warning will be recorded.
         */
        double GetWhiteBalanceGreen() override;

        /**
         * @brief Auto adjust the exposure for once.
         * @details
         *  The adjusted exposure value will be saved into configuration.
         */
        bool AutoAdjustExposure() override;

        /**
         * @brief Auto adjust the gain for once.
         * @details
         *  The adjusted gain value will be saved into configuration .
         */
        bool AutoAdjustGain() override;

        /**
         * @brief Auto adjust white balance for once.
         * @details
         *  The adjusted white balance value not be saved into configuration.
         */
        bool AutoAdjustWhiteBalance() override;
    };
}
