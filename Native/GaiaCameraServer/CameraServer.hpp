#pragma once

#include <memory>
#include <string>
#include <atomic>
#include <list>
#include <sw/redis++/redis++.h>
#include <GaiaBackground/GaiaBackground.hpp>
#include <GaiaLogClient/GaiaLogClient.hpp>
#include <GaiaConfigurationClient/GaiaConfigurationClient.hpp>
#include <GaiaNameClient/GaiaNameClient.hpp>
#include <GaiaSharedMemory/GaiaSharedMemory.hpp>

#include "CameraDriverInterface.hpp"

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
    class CameraServer
    {
        /// Allow camera interface to invoke basic functions.
        friend class CameraDriverInterface;

    private:
        /// Driver for the specific camera.
        std::unique_ptr<CameraDriverInterface> CameraDriver;

        /// Life flag for the main loop.
        std::atomic<bool> LifeFlag {false};

    protected:
        /// Connection to the Redis server.
        std::shared_ptr<sw::redis::Redis> Connection {nullptr};
        /// Subscriber gotten from the Redis connection.
        std::unique_ptr<sw::redis::Subscriber> Subscriber {nullptr};
        /// Client for name service.
        std::unique_ptr<NameService::NameClient> NameResolver {nullptr};
        /// Client for log service.
        std::unique_ptr<LogService::LogClient> Logger {nullptr};
        /// Client for configuration service.
        std::unique_ptr<ConfigurationService::ConfigurationClient> Configurator {nullptr};

        /// Execute the command.
        void HandleCommand(const std::string& command);

        /// Update the timestamp of the target picture.
        void UpdatePictureTimestamp(const std::string& picture_name);

        /// Update the chain id of the shared picture block.
        void UpdatePictureBlockID(const std::string& picture_name, unsigned int chain_id);

        /// Update the total amount of swap chain blocks.
        void UpdatePictureBlocksCount(const std::string& picture_name, unsigned int blocks_count);

    public:
        /// Whether user require the camera to flip the picture or not.
        bool RequiredFlip {false};

        /// Get logger of this server.
        [[nodiscard]] inline LogService::LogClient* GetLogger() const
        {
            return Logger.get();
        }
        /// Get configurator of this server.
        [[nodiscard]] inline ConfigurationService::ConfigurationClient* GetConfigurator() const
        {
            return Configurator.get();
        }
        /// Get connection to the Redis server.
        [[nodiscard]] inline sw::redis::Redis* GetDatabase()
        {
            return Connection.get();
        }

        /**
         * @brief Connect to the Redis server and open the camera device on the given index.
         * @param camera_driver Camera driver instance.
         * @param device_index Index of the camera device to open, beginning from 0.
         * @param port Port of the Redis server.
         * @param ip IP address of the Redis server.
         */
        explicit CameraServer(
                std::unique_ptr<CameraDriverInterface>&& camera_driver,
                unsigned int device_index = 0,
                unsigned int port = 6379, const std::string& ip = "127.0.0.1");

        /// Destructor which will stop the updater.
        ~CameraServer();

        /**
         * @brief Open the camera device and launch the server.
         * @details
         *  This function will block the invoker thread until receive "shutdown" command.
         */
        void Launch();
    };
}