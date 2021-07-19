#pragma once

#include <memory>
#include <chrono>
#include <GaiaSharedPicture/GaiaSharedPicture.hpp>
#include <GaiaCameraServer/GaiaCameraServer.hpp>
#include <GaiaBackground/GaiaBackground.hpp>
#include <opencv2/opencv.hpp>

namespace Gaia::CameraService
{
    class VideoDriver : public CameraDriverInterface
    {
    private:

        Gaia::Background::BackgroundWorker Updater;

        const unsigned int SwapChainTotalCount {10};
        unsigned int SwapChainReadyIndex {0};

        unsigned int CurrentFrameIndex {0};
        unsigned int TotalFrameCount {0};

        std::unique_ptr<cv::VideoCapture> Video;

        /// Writer for writing shared picture.
        std::vector<std::unique_ptr<SharedPicture::PictureWriter>> Writers;

        /// Time point of last receive picture event, used for judging whether this camera is alive or not.
        std::atomic<std::chrono::steady_clock::time_point> LastReceiveTimePoint {std::chrono::steady_clock::now()};

    public:
        /// Default constructor.
        VideoDriver();
        /// Auto close the camera.
        ~VideoDriver() override;

        void OnPictureCapture();

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

        /// Check whether this camera is alive or not.
        bool IsAlive() override;

        /**
         * @brief Get the exposure of the camera.
         * @return Microseconds of the exposure time.
         */
        bool SetExposure(unsigned int microseconds) override;

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
         */
        bool SetWhiteBalanceBlue(double ratio) override;

        /// Get blue channel value of the white balance.
        double GetWhiteBalanceBlue() override;

        /**
         * @brief Set green channel value of the white balance.
         * @param ratio Value of the target channel.
         */
        bool SetWhiteBalanceGreen(double ratio) override;

        /// Get green channel value of the white balance.
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
