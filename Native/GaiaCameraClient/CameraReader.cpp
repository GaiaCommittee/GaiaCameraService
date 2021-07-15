#include "CameraReader.hpp"

#include <exception>

namespace Gaia::CameraService
{

    CameraReader::CameraReader(std::shared_ptr<sw::redis::Redis> connection, const std::string& device_name,
                               const std::string& picture_name) :
        Connection(std::move(connection)), MemoryBlockName(device_name + "." + picture_name),
        StatusTimestampKeyName("cameras/" + device_name + "/pictures/" + picture_name + "/timestamp"),
        StatusFPSKeyName("cameras/" + device_name + "/pictures/" + picture_name + "/timestamp"),
        StatusBlockIDKeyName("cameras/" + device_name + "/pictures/" + picture_name + "/id"),
        DeviceName(device_name), PictureName(picture_name)
    {
        InitializeReaders(device_name, picture_name);
    }

    /// Copy constructor.
    CameraReader::CameraReader(const CameraReader &target) :
        Connection(target.Connection), MemoryBlockName(target.MemoryBlockName),
        StatusTimestampKeyName(target.StatusTimestampKeyName),
        StatusFPSKeyName(target.StatusFPSKeyName),
        DeviceName(target.DeviceName), PictureName(target.PictureName)
    {
        InitializeReaders(DeviceName, PictureName);
    }

    /// Read the current picture.
    cv::Mat CameraReader::Read() const
    {
        auto block_id_text = Connection->get(StatusBlockIDKeyName);
        if (!block_id_text.has_value())
            throw std::runtime_error("Picture swap chain id is empty.");
        auto block_id = std::stoi(*block_id_text);

        if (block_id >= Readers.size()) throw std::runtime_error("Swap chain block ID out of range.");
        return Readers[block_id]->Read();
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

    /// Initialize the readers.
    void CameraReader::InitializeReaders(const std::string& device_name, const std::string& picture_name)
    {
        auto count_text = Connection->get("cameras/" + device_name + "/pictures/" + picture_name + "/blocks");
        if (!count_text.has_value()) throw std::runtime_error("Picture " + picture_name + " of camera " +
            device_name + " has not defined blocks count.");
        auto count = std::stoi(*count_text);

        Readers.clear();
        Readers.reserve(count);
        for (auto chain_index = 0; chain_index < count; ++chain_index)
        {
            auto reader = std::make_unique<SharedPicture::PictureReader>(
                    device_name + "." + picture_name +
                    "." + std::to_string(chain_index));
            Readers.emplace_back(std::move(reader));
        }
    }
}