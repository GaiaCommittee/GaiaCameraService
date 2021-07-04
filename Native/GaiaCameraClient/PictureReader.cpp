#include "PictureReader.hpp"

#include <exception>

namespace Gaia::CameraService
{

    PictureReader::PictureReader(
            std::shared_ptr<sw::redis::Redis> connection,
            const std::string& information_prefix,
            const std::string& memory_block_name) :
            Connection(std::move(connection)),
            InformationPrefix(information_prefix), MemoryBlockName(memory_block_name),
            TimestampName(information_prefix + "timestamp")
    {
        auto optional_width = Connection->get(information_prefix + "width");
        if (optional_width) WidthSource = std::stoi(*optional_width);
        auto optional_height = Connection->get(information_prefix + "height");
        if (optional_height) HeightSource = std::stoi(*optional_height);
        auto optional_channels = Connection->get(information_prefix + "channels");
        if (optional_channels) ChannelsSource = std::stoi(*optional_channels);
        FormatSource = Connection->get(information_prefix + "format").value_or("Unknown");

        if (Format == "8U"){
            MatFormat = CV_8UC(Channels);
        }else if (Format == "8S"){
            MatFormat = CV_8SC(Channels);
        }else if (Format == "16U"){
            MatFormat = CV_16UC(Channels);
        }else if (Format == "16S"){
            MatFormat = CV_16SC(Channels);
        }else if (Format == "16F"){
            MatFormat = CV_16FC(Channels);
        }else if (Format == "32S"){
            MatFormat = CV_32SC(Channels);
        }else if (Format == "32F"){
            MatFormat = CV_32FC(Channels);
        }else if (Format == "64F"){
            MatFormat = CV_64FC(Channels);
        }

        PictureBlock.Open(memory_block_name);
        if (PictureBlock.GetSize() < Width * Height * Channels)
        {
            throw std::runtime_error("Shared memory block '" + memory_block_name +
                                     "' with the size of " + std::to_string(PictureBlock.GetSize()) + " is smaller than that needed:" +
                                     std::to_string(Width) + "*" + std::to_string(Height) + "*" + std::to_string(Channels));
        }
    }

    /// Copy constructor.
    PictureReader::PictureReader(const PictureReader &reader) :
            Connection(reader.Connection),
            TimestampName(reader.TimestampName), InformationPrefix(reader.InformationPrefix),
            MemoryBlockName(reader.MemoryBlockName),
            WidthSource(reader.WidthSource), HeightSource(reader.HeightSource),
            ChannelsSource(reader.ChannelsSource), FormatSource(reader.FormatSource),
            MatFormat(reader.MatFormat)
    {
        PictureBlock.Open(MemoryBlockName);
    }

    /// Read the current picture.
    cv::Mat PictureReader::Read() const
    {
        return cv::Mat(cv::Size(static_cast<int>(Width), static_cast<int>(Height)),
                       MatFormat, PictureBlock.GetPointer());
    }

    /// Get the timestamp of the current picture.
    std::chrono::system_clock::time_point PictureReader::ReadTimestamp()
    {
        // Restore timestamp according to the milliseconds since epoch.
        return std::chrono::system_clock::time_point(std::chrono::milliseconds(ReadMillisecondsTimestamp()));
    }

    /// Read the timestamp in format of milliseconds since epoch.
    long PictureReader::ReadMillisecondsTimestamp()
    {
        auto timestamp_string = Connection->get(TimestampName);
        if (timestamp_string)
        {
            return std::stol(*timestamp_string);
        }
        return 0;
    }
}