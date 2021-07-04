#include "ZedClient.hpp"
#include <sstream>

namespace Gaia::CameraService
{
    /// Constructor
    ZedClient::ZedClient() : CameraClient("zed", 0)
    {}

    /// Convert a string of float3 into  a float3 vector.
    sl::float3 ConvertStringToFloat3(const std::string& text)
    {
        sl::float3 data {0, 0, 0};
        auto comma1_position = text.find(',');
        data.x = std::stof(text.substr(0, comma1_position));
        auto comma2_position = text.find(',', comma1_position + 1);
        data.y = std::stof(text.substr(comma1_position + 1, comma2_position));
        data.z = std::stof(text.substr(comma2_position + 1));
        return data;
    }

    /// Get magnetometer data.
    sl::float3 ZedClient::GetMagneticField()
    {
        auto value_text = Connection->get("cameras/" + DeviceName + "/status/magnetic_field");
        if (value_text)
        {
            return ConvertStringToFloat3(*value_text);
        }
        return {0.0, 0.0, 0.0};
    }

    /// Get relative altitude data.
    float ZedClient::GetRelativeAltitude()
    {
        auto value_text = Connection->get("cameras/" + DeviceName + "/status/relative_altitude");
        if (value_text)
        {
            return std::stof(*value_text);
        }
        return 0.0;
    }

    /// Get linear acceleration applied to the camera.
    sl::float3 ZedClient::GetLinearAcceleration()
    {
        auto value_text = Connection->get("cameras/" + DeviceName + "/status/linear_acceleration");
        if (value_text)
        {
            return ConvertStringToFloat3(*value_text);
        }
        return {0.0, 0.0, 0.0};
    }

    /// Get angular velocity data.
    sl::float3 ZedClient::GetAngularVelocity()
    {
        auto value_text = Connection->get("cameras/" + DeviceName + "/status/angular_velocity");
        if (value_text)
        {
            return ConvertStringToFloat3(*value_text);
        }
        return {0.0, 0.0, 0.0};
    }

    /// Get the orientation angle of this camera.
    sl::float3 ZedClient::GetOrientation()
    {
        auto value_text = Connection->get("cameras/" + DeviceName + "/status/orientation");
        if (value_text)
        {
            return ConvertStringToFloat3(*value_text);
        }
        return {0.0, 0.0, 0.0};
    }
}