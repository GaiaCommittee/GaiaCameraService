#pragma once

#include <string>
#include <memory>
#include <sw/redis++/redis++.h>
#include <GaiaSharedMemory/GaiaSharedMemory.hpp>
#include <GaiaSharedPicture/GaiaSharedPicture.hpp>
#include <opencv2/opencv.hpp>

namespace Gaia::CameraService
{
    /**
     * @brief Picture reader can restore the cv::Mat of the picture and
     *        get information about this picture before reading.
     */
    class CameraReader
    {
        friend class CameraClient;

    protected:
        /// Connection to the Redis server.
        std::shared_ptr<sw::redis::Redis> Connection;
        /// Reader for the picture in a shared memory block.
        std::unique_ptr<SharedPicture::PictureReader> Reader;

        /// Name of the memory block to store the picture.
        const std::string MemoryBlockName;
        /// Name prefix for information items of this picture.
        const std::string StatusTimestampKeyName;
        /// Name for the timestamp of this picture.
        const std::string StatusFPSKeyName;

    public:
        /**
         * @brief Connect to the shared memory block with the given name.
         * @param connection Connection to the Redis server.
         * @param device_name Name of the camera device.
         * @param picture_name Name of the picture.
         */
        explicit CameraReader(
                std::shared_ptr<sw::redis::Redis> connection,
                const std::string& device_name,
                const std::string& picture_name);
        /// Copy constructor.
        CameraReader(const CameraReader& reader);
        /// Move constructor.
        CameraReader(CameraReader&& reader) = default;

        /// Read the data matrix of this picture.
        [[nodiscard]] cv::Mat Read() const;
        /// Read the timestamp in format of milliseconds since epoch.
        [[nodiscard]] long ReadMillisecondsTimestamp();
        /// Read the timestamp of this picture.
        [[nodiscard]] std::chrono::system_clock::time_point ReadTimestamp();
    };
}