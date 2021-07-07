#pragma once

#include <memory>
#include <GaiaSharedPicture/GaiaSharedPicture.hpp>
#include <GaiaCameraServer/GaiaCameraServer.hpp>

namespace Gaia::CameraService
{
    class DahengDriver : public CameraDriverInterface
    {
    private:
        // Camera handle gotten from SDK.
        void* DeviceHandle {nullptr};
        /// Count of pictures captured by the camera.
        std::atomic<unsigned int> ReceivedPicturesCount {0};
        /// Writer for writing shared picture.
        std::unique_ptr<SharedPicture::PictureWriter> Writer {nullptr};

    public:
        /// Default constructor.
        DahengDriver();
        /// Auto close the camera.
        ~DahengDriver() override;

        /**
         * @brief Invoked when a new picture is captured by the camera.
         * @param parameters Pointer to the parameters package.
         */
        void OnPictureCapture(void* parameters);

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