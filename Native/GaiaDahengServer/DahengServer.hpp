#pragma once

#include <memory>
#include <string>
#include <atomic>
#include <list>
#include <sw/redis++/redis++.h>
#include <GaiaBackground/GaiaBackground.hpp>
#include <GaiaLogClient/GaiaLogClient.hpp>
#include <GaiaConfigurationClient/GaiaConfigurationClient.hpp>
#include <GaiaSharedMemory/GaiaSharedMemory.hpp>

namespace Gaia::CameraService
{
    /**
     * @brief This server provides camera control functions through Redis server.
     * @details
     *  The picture captured by the camera will be stored in the shared memory named for example
     *  "daheng_camera.0.main" when the device index is 0.
     *  The name "daheng_camera.0" will be added to the set named "cameras",
     *  settings are "exposure", "gain", etc.
     *  Properties can be controlled through the Redis channel named "cameras/daheng_camera.0/command".
     *  Status will be stored in the channel named "cameras/daheng_camera.0/status", for example,
     *  FPS as "cameras/daheng_camera.0/status/fps".
     *  Names of pictures like "main" will be added to the set named
     *  "cameras/daheng_camera.0/pictures",
     *  information of pictures will be stored as "cameras/daheng_camera.0/pictures/main/width",
     *  "cameras/daheng_camera.0/pictures/main/height", "cameras/daheng_camera.0/pictures/main/channels",
     *  and "cameras/daheng_camera.0/pictures/main/format".
     *  Formats are "BGR", "Gray", "BayerRG", "BayerBG", etc.
     *  The default destination format of pictures sent from this server is "BGR".
     */
    class DahengServer
    {
    private:
        /// Handle for controlling the device.
        void* DeviceHandle {nullptr};

        /// Index of the camera device.
        const unsigned int DeviceIndex;
        /// Generated name for the device, for example, "daheng_camera.0"
        const std::string DeviceName;
        /// Device format.
        int DeviceFormat;

        /// Count of pictures captured by the camera.
        std::atomic<unsigned int> ReceivedPicturesCount {0};

        /// Shared memory for the captured picture.
        SharedMemory::SourceMemory PictureSource;

        /// Life flag for the main loop.
        std::atomic<bool> LifeFlag {false};

    protected:
        /// Connection to the Redis server.
        std::shared_ptr<sw::redis::Redis> Connection;
        /// Subscriber gotten from the Redis connection.
        std::unique_ptr<sw::redis::Subscriber> Subscriber;
        /// Client for log service.
        std::unique_ptr<LogService::LogClient> Logger;
        /// Client for configuration service.
        std::unique_ptr<ConfigurationService::ConfigurationClient> Configurator;

        /// Execute the command.
        void HandleCommand(const std::string& command);

        /**
         * @brief Update status information.
         * @details This function will be called frequently (every 2 seconds).
         */
        void UpdateStatus();

    public:
        /**
         * @brief Connect to the Redis server and open the camera device on the given index.
         * @param device_index Index of the camera device to open, beginning from 0.
         * @param port Port of the Redis server.
         * @param ip IP address of the Redis server.
         */
        explicit DahengServer(unsigned int device_index = 0,
                     unsigned int port = 6379, const std::string& ip = "127.0.0.1",
                     const std::string& format = "bayer_rg");

        /// Destructor which will stop the updater.
        ~DahengServer();

        /**
         * @brief Invoked when a new picture is captured by the camera.
         * @param data Address of the raw picture captured by the camera.
         * @param width Width of the raw picture.
         * @param height Height of the raw picture.
         */
        void OnPictureCapture(void* data, int width, int height);

        /**
         * @brief Open the camera device and launch the server.
         * @details
         *  This function will block the invoker thread until receive "shutdown" command.
         */
        void Launch();

        /// Open the camera on the given index and start acquisition.
        void Open(unsigned int device_index);

        /// Close the camera on the given index and stop acquisition.
        void Close();

        /**
         * @brief Set the exposure of the camera.
         * @param microseconds Exposure time in microseconds.
         */
        bool SetExposure(unsigned int microseconds);
        /**
         * @brief Set the digital gain of the camera.
         * @param gain Value of the digital gain.
         */
        bool SetGain(double gain);
        /**
         * @brief Set red channel value of the white balance.
         * @param ratio Value of the target channel.
         */
        bool SetWhiteBalanceRed(double ratio);
        /**
         * @brief Set blue channel value of the white balance.
         * @param ratio Value of the target channel.
         */
        bool SetWhiteBalanceBlue(double ratio);
        /**
         * @brief Set green channel value of the white balance.
         * @param ratio Value of the target channel.
         */
        bool SetWhiteBalanceGreen(double ratio);
    };
}