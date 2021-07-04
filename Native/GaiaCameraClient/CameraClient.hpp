#pragma once

#include <string>
#include <memory>
#include <unordered_set>
#include <sw/redis++/redis++.h>
#include <GaiaSharedMemory/GaiaSharedMemory.hpp>

#include "PictureReader.hpp"

namespace Gaia::CameraService
{
    /**
     * @brief Client for camera service, provides function to get the picture in OpenCV matrix format.
     */
    class CameraClient
    {
    protected:
        /// Generated name of the camera device according to the camera type and device index.
        const std::string DeviceName;
        /// Name of the channel for camera control.
        const std::string CommandChannelName;
        /// Name prefix for configuration items of the camera.
        const std::string ConfigurationPrefix;

    protected:
        /// Connection to the Redis server.
        std::shared_ptr<sw::redis::Redis> Connection;

    public:
        /**
         * @brief Reuse the connection to connect to a Redis server and connect to the given camera.
         * @param camera Type name of the camera.
         * @param index Index of the camera to open.
         * @param connection Connection to a Redis server.
         */
        CameraClient(const std::string& camera_type, unsigned int index,
                     std::shared_ptr<sw::redis::Redis> connection);
        /**
         * @brief Establish a connection to the Redis server and connect to the given camera.
         * @param camera_type Type name of the camera.
         * @param index Index of the camera to open.
         * @param port Port of the Redis server.
         * @param ip IP address of the Redis server.
         */
        CameraClient(const std::string& camera_type, unsigned int index,
                     unsigned int port = 6379, const std::string& ip = "127.0.0.1");

        /// Get names set of pictures.
        std::unordered_set<std::string> GetPictures();

        /**
         * @brief Get the reader for a picture with the given name.
         * @param picture_name Name of the picture.
         * @return Reader for a picture with the given name.
         */
        PictureReader GetReader(const std::string& picture_name);

        /// Get current frames per second.
        [[nodiscard]] int GetFPS();

        /**
         * @brief Set the exposure of the camera.
         * @param microseconds Exposure time in microseconds.
         */
        void SetExposure(unsigned int microseconds);
        /**
         * @brief Set the digital gain of the camera.
         * @param gain Value of the digital gain.
         */
        void SetGain(double gain);
        /**
         * @brief Set red channel value of the white balance.
         * @param red_ratio Value of the red channel.
         * @param green_ratio Value of the green channel.
         * @param blue_ratio Value of the blue channel.
         */
        void SetWhiteBalance(double red_ratio, double green_ratio, double blue_ratio);
        /**
         * @brief Auto adjust the exposure for once.
         * @details
         *  The adjusted exposure value will be saved into configuration.
         */
        void AutoAdjustExposure();
        /**
         * @brief Auto adjust the gain for once.
         * @details
         *  The adjusted gain value will be saved into configuration .
         */
        void AutoAdjustGain();
        /**
         * @brief Auto adjust white balance for once.
         * @details
         *  The adjusted white balance value not be saved into configuration.
         */
        void AutoAdjustWhiteBalance();
    };
}