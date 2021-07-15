#include "CameraDriverInterface.hpp"

#include <utility>
#include "CameraServer.hpp"

namespace Gaia::CameraService
{
    /// Constructor
    CameraDriverInterface::CameraDriverInterface(std::string type_name) :
        DeviceTypeName(std::move(type_name))
    {}

    /// Generate picture shared memory block name.
    std::string CameraDriverInterface::GetPictureBlockName(const std::string &picture_name, unsigned int block_id)
    {
        return DeviceName + "." + picture_name + "." + std::to_string(block_id);
    }

    /// Update the timestamp of the given picture.
    void CameraDriverInterface::UpdatePictureTimestamp(const std::string &picture_name)
    {
        if (Server)
        {
            Server->UpdatePictureTimestamp(picture_name);
        }
    }

    /// Update the block ID of the given picture.
    void CameraDriverInterface::UpdatePictureBlockID(const std::string& picture_name, unsigned int id)
    {
        if (Server)
        {
            Server->UpdatePictureBlockID(picture_name, id);
        }
    }

    /// Get the logger of the host server.
    LogService::LogClient* CameraDriverInterface::GetLogger() const
    {
        if (Server)
        {
            return Server->GetLogger();
        }
        return nullptr;
    }

    /// Get the configurator of the host server.
    ConfigurationService::ConfigurationClient* CameraDriverInterface::GetConfigurator() const
    {
        if (Server)
        {
            return Server->GetConfigurator();
        }
        return nullptr;
    }

    /// Get connection to the Redis server.
    sw::redis::Redis *CameraDriverInterface::GetDatabase() const
    {
        if (Server)
        {
            return Server->GetDatabase();
        }
        return nullptr;
    }

    /// Initialize this camera.
    void CameraDriverInterface::Initialize(unsigned int device_index, CameraServer *server)
    {
        DeviceIndexSource = device_index;
        DeviceNameSource = DeviceTypeName + "." + std::to_string(DeviceIndexSource);
        Server = server;
    }

    /// Check whether the server is required to flip the picture or not.
    bool CameraDriverInterface::IsRequiredFlip() const
    {
        if (Server)
        {
            return Server->RequiredFlip;
        }
        return false;
    }

    /// Update the total amount of swap chain blocks.
    void CameraDriverInterface::UpdatePictureBlocksCount(const std::string &picture_name, unsigned int blocks_count)
    {
        if (Server)
        {
            Server->UpdatePictureBlocksCount(picture_name, blocks_count);
        }
    }
}