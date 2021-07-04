#include "CameraInterface.hpp"

#include <utility>
#include "CameraServer.hpp"

namespace Gaia::CameraService
{
    /// Constructor
    CameraInterface::CameraInterface(std::string type_name) :
        DeviceTypeName(std::move(type_name))
    {}

    /// Generate picture shared memory block name.
    std::string CameraInterface::GetPictureBlockName(const std::string &picture_name)
    {
        return DeviceName + "." + picture_name;
    }

    /// Update the timestamp of the given picture.
    void CameraInterface::UpdatePictureTimestamp(const std::string &picture_name)
    {
        if (Server)
        {
            Server->UpdatePictureTimestamp(picture_name);
        }
    }

    /// Get the logger of the host server.
    LogService::LogClient* CameraInterface::GetLogger() const
    {
        if (Server)
        {
            return Server->GetLogger();
        }
        return nullptr;
    }

    /// Get the configurator of the host server.
    ConfigurationService::ConfigurationClient* CameraInterface::GetConfigurator() const
    {
        if (Server)
        {
            return Server->GetConfigurator();
        }
        return nullptr;
    }

    /// Get connection to the Redis server.
    sw::redis::Redis *CameraInterface::GetDatabase() const
    {
        if (Server)
        {
            return Server->GetDatabase();
        }
        return nullptr;
    }

    /// Initialize this camera.
    void CameraInterface::Initialize(unsigned int device_index, CameraServer *server)
    {
        DeviceIndexSource = device_index;
        DeviceNameSource = DeviceTypeName + "." + std::to_string(DeviceIndexSource);
        Server = server;
    }
}