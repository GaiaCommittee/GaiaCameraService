#pragma once

#include <string>
#include <memory>
#include <sw/redis++/redis++.h>
#include <GaiaSharedMemory/GaiaSharedMemory.hpp>
#include <opencv2/opencv.hpp>

namespace Gaia::CameraService
{
    /**
     * @brief Picture reader can restore the cv::Mat of the picture and
     *        get information about this picture before reading.
     */
    class PictureReader
    {
        friend class CameraClient;

    protected:
        /// Connection to the Redis server.
        std::shared_ptr<sw::redis::Redis> Connection;
        /// Memory block of the shared picture.
        SharedMemory::ImageMemory PictureBlock;

        /// Name of the memory block to store the picture.
        const std::string MemoryBlockName;
        /// Name prefix for information items of this picture.
        const std::string InformationPrefix;
        /// Name for the timestamp of this picture.
        const std::string TimestampName;
        /// Mat format for OpenCV.
        int MatFormat;

        /// Source of picture width property.
        unsigned int WidthSource {0};
        /// Source of picture height property.
        unsigned int HeightSource {0};
        /// Source of picture channels property.
        unsigned int ChannelsSource {0};
        /// Source of picture format property.
        std::string PixelFormatSource {"Unknown"};
    public:
        /**
         * @brief Connect to the shared memory block with the given name.
         * @param connection Connection to the Redis server.
         * @param memory_block_name Name of the shared memory block to connect to.
         */
        explicit PictureReader(
                std::shared_ptr<sw::redis::Redis> connection,
                const std::string& information_prefix,
                const std::string& memory_block_name);
        /// Copy constructor.
        PictureReader(const PictureReader& reader);
        /// Move constructor.
        PictureReader(PictureReader&& reader) = default;

        /// Width of the picture.
        const decltype(WidthSource)& Width {WidthSource};
        /// Height of the picture.
        const decltype(HeightSource)& Height {HeightSource};
        /// Channels of the picture.
        const decltype(ChannelsSource)& Channels {ChannelsSource};
        /// Format of the picture.
        const decltype(PixelFormatSource)& PixelFormat {PixelFormatSource};

        /// Read the data matrix of this picture.
        [[nodiscard]] cv::Mat Read() const;
        /// Read the timestamp in format of milliseconds since epoch.
        [[nodiscard]] long ReadMillisecondsTimestamp();
        /// Read the timestamp of this picture.
        [[nodiscard]] std::chrono::system_clock::time_point ReadTimestamp();
    };
}