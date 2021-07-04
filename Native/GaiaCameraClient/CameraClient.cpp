#include "CameraClient.hpp"

namespace Gaia::CameraService
{
    /// Establish a connection to the Redis server.
    CameraClient::CameraClient(const std::string& camera_type, unsigned int index,
                               unsigned int port, const std::string &ip) :
            CameraClient(camera_type, index,
                         std::make_shared<sw::redis::Redis>("tcp://" + ip + ":" + std::to_string(port)))
    {}

    /// Reuse the connection to a Redis server.
    CameraClient::CameraClient(const std::string& camera_type, unsigned int index,
                               std::shared_ptr<sw::redis::Redis> connection) :
           Connection(std::move(connection)), DeviceName(camera_type + "." + std::to_string(index)),
           CommandChannelName("cameras/" + DeviceName + "/command"),
           ConfigurationPrefix("configurations/" + DeviceName + "/")
    {}

    /// Get names set of pictures.
    std::unordered_set<std::string> CameraClient::GetPictures()
    {
        std::unordered_set<std::string> pictures;
        Connection->smembers("cameras/" + DeviceName + "/pictures", std::inserter(pictures, pictures.end()));
        return pictures;
    }

    /// Get a reader for the picture with the given name.
    PictureReader CameraClient::GetReader(const std::string &picture_name)
    {
        return PictureReader(Connection,
                             "cameras/" + DeviceName + "/pictures/" + picture_name + "/",
                             DeviceName + "." + picture_name);
    }

    /// Get current frames per second.
    int CameraClient::GetFPS()
    {
        auto optional_fps = Connection->get("cameras/" + DeviceName + "/status/fps");
        if (optional_fps) return std::stoi(*optional_fps);
        return 0;
    }

    /// Set the exposure of the camera.
    void CameraClient::SetExposure(unsigned int milliseconds)
    {
        Connection->set(ConfigurationPrefix + "Exposure", std::to_string(milliseconds));
        Connection->publish(CommandChannelName, "update_exposure");
    }

    /// Set the digital gain of the camera.
    void CameraClient::SetGain(double gain)
    {
        Connection->set(ConfigurationPrefix + "Gain", std::to_string(gain));
        Connection->publish(CommandChannelName, "update_gain");
    }

    /// Set values of the white balance.
    void CameraClient::SetWhiteBalance(double red_ratio, double green_ratio, double blue_ratio)
    {
        Connection->set(ConfigurationPrefix + "WhiteBalanceRed", std::to_string(red_ratio));
        Connection->set(ConfigurationPrefix + "WhiteBalanceGreen", std::to_string(green_ratio));
        Connection->set(ConfigurationPrefix + "WhiteBalanceBlue", std::to_string(blue_ratio));
        Connection->publish(CommandChannelName, "update_white_balance");
    }

    /// Auto adjust the exposure for once.
    void CameraClient::AutoAdjustExposure()
    {
        Connection->publish(CommandChannelName, "auto_exposure");
    }

    /// Auto adjust the gain for once.
    void CameraClient::AutoAdjustGain()
    {
        Connection->publish(CommandChannelName, "auto_gain");
    }

    /// Auto adjust white balance for once.
    void CameraClient::AutoAdjustWhiteBalance()
    {
        Connection->publish(CommandChannelName, "auto_white_balance");
    }
}