#include "CameraReader.hpp"

#include <exception>

namespace Gaia::CameraService
{

    CameraReader::CameraReader(std::shared_ptr<sw::redis::Redis> connection, const std::string& device_name,
                               const std::string& picture_name) :
        Connection(std::move(connection)), MemoryBlockName(device_name + "." + picture_name),
        StatusTimestampKeyName("cameras/" + device_name + "/pictures/" + picture_name + "/timestamp"),
        StatusFPSKeyName("cameras/" + device_name + "/pictures/" + picture_name + "/timestamp")
    {
        Reader = std::make_unique<SharedPicture::PictureReader>(MemoryBlockName);
    }

    /// Copy constructor.
    CameraReader::CameraReader(const CameraReader &target) :
        Connection(target.Connection), MemoryBlockName(target.MemoryBlockName),
        StatusTimestampKeyName(target.StatusTimestampKeyName),
        StatusFPSKeyName(target.StatusFPSKeyName)
    {
        Reader = std::make_unique<SharedPicture::PictureReader>(MemoryBlockName);
    }

    /// Read the current picture.
    cv::Mat CameraReader::Read() const
    {
        if (Reader)
        {
            return Reader->Read();
        }
        else
        {
            throw std::runtime_error("Failed to read picture: reader is null.");
        }
    }

    /// Get the timestamp of the current picture.
    std::chrono::system_clock::time_point CameraReader::ReadTimestamp()
    {
        // Restore timestamp according to the milliseconds since epoch.
        return std::chrono::system_clock::time_point(std::chrono::milliseconds(ReadMillisecondsTimestamp()));
    }

    /// Read the timestamp in format of milliseconds since epoch.
    long CameraReader::ReadMillisecondsTimestamp()
    {
        auto timestamp_string = Connection->get(StatusTimestampKeyName);
        if (timestamp_string)
        {
            return std::stol(*timestamp_string);
        }
        return 0;
    }
}