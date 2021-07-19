#include "VideoDriver.hpp"

#include <MvCameraControl.h>

namespace Gaia::CameraService
{
    /// Constructor.
    VideoDriver::VideoDriver() :
            CameraDriverInterface("hik"),
            Updater([this](const std::atomic_bool& flag){
                while (flag)
                {
                    this->OnPictureCapture();
                    cv::waitKey(15);
                }
            })
    {}

    /// Destructor which will automatically close the device.
    VideoDriver::~VideoDriver()
    {
        Close();
    }

    /// Invoked when a new picture is captured by the camera.
    void VideoDriver::OnPictureCapture()
    {
        RetrievedPicturesCount++;

        cv::Mat picture;
        (*Video) >> picture;
        ++CurrentFrameIndex;
        if (CurrentFrameIndex >= TotalFrameCount - 1)
        {
            CurrentFrameIndex = 0;
            Video->set(cv::CAP_PROP_POS_FRAMES, 0);
        }
        auto* writer = Writers[SwapChainReadyIndex].get();
        writer->Write(picture);
        UpdatePictureBlockID(std::string("main"), SwapChainReadyIndex);
        ++SwapChainReadyIndex;
        if (SwapChainReadyIndex >= SwapChainTotalCount)
        {
            SwapChainReadyIndex = 0;
        }
        UpdatePictureTimestamp("main");

        LastReceiveTimePoint = std::chrono::steady_clock::now();
    }

    /// Open the camera.
    void VideoDriver::Open()
    {
        Video = std::make_unique<cv::VideoCapture>(DeviceName);

        if (!Video->isOpened())
        {
            throw std::runtime_error("Can not open video " + DeviceName);
        }

        CurrentFrameIndex = 0;
        TotalFrameCount = Video->get(cv::CAP_PROP_FRAME_COUNT);

        // Prepare shared memory.
        UpdatePictureBlocksCount("main", SwapChainTotalCount);
        Writers.reserve(SwapChainTotalCount);
        SharedPicture::PictureHeader picture_header;
        picture_header.PixelType = SharedPicture::PictureHeader::PixelTypes::Unsigned;
        picture_header.PixelBits = SharedPicture::PictureHeader::PixelBitSizes::Bits8;
        picture_header.Channels = 3;
        picture_header.Width = GetPictureWidth();
        picture_header.Height = GetPictureHeight();
        for (auto chain_index = 0; chain_index < SwapChainTotalCount; ++chain_index)
        {
            auto writer = std::make_unique<SharedPicture::PictureWriter>(
                    DeviceName + ".main." + std::to_string(chain_index),
                    static_cast<long>(GetPictureWidth() * GetPictureHeight() * 3), true);
            writer->SetHeader(picture_header);
            Writers.emplace_back(std::move(writer));
        }

        LastReceiveTimePoint = std::chrono::steady_clock::now();
    }

    /// Close the opened camera device.
    void VideoDriver::Close()
    {
        Video.reset();
        if (!Writers.empty()) Writers.clear();
    }

    /// Check time point to judge whether this camera is alive or not.
    bool VideoDriver::IsAlive()
    {
        return true;
    }

    /// Set the exposure of the camera.
    bool VideoDriver::SetExposure(unsigned int microseconds)
    {
        return false;
    }

    /// Get exposure time.
    unsigned int VideoDriver::GetExposure()
    {
        return 0;
    }

    /// Set the digital gain of the camera.
    bool VideoDriver::SetGain(double gain)
    {
        return false;
    }

    /// Get digital gain.
    double VideoDriver::GetGain()
    {
        return 0.0;
    }

    /// Set red channel value of the white balance.
    bool VideoDriver::SetWhiteBalanceRed(double ratio)
    {
        return false;
    }

    /// Get white balance red channel value.
    double VideoDriver::GetWhiteBalanceRed()
    {
        return 0.0;
    }

    /// Set blue channel value of the white balance.
    bool VideoDriver::SetWhiteBalanceBlue(double ratio)
    {
        return false;
    }

    /// Get white balance blue channel value.
    double VideoDriver::GetWhiteBalanceBlue()
    {
        return 0.0;
    }

    /// Set green channel value of the white balance.
    bool VideoDriver::SetWhiteBalanceGreen(double ratio)
    {
        return false;
    }

    /// Get white balance green channel value.
    double VideoDriver::GetWhiteBalanceGreen()
    {
        return 0.0;
    }

    /// Auto adjust the exposure for once.
    bool VideoDriver::AutoAdjustExposure()
    {
        return false;
    }

    /// Auto adjust the gain for once.
    bool VideoDriver::AutoAdjustGain()
    {
        return false;
    }

    /// Auto adjust white balance for once.
    bool VideoDriver::AutoAdjustWhiteBalance()
    {
        return false;
    }

    /// Get width of the picture.
    long VideoDriver::GetPictureWidth()
    {
        return false;
    }

    /// Get height of the picture.
    long VideoDriver::GetPictureHeight()
    {
        return false;
    }

    /// Get picture names list.
    std::vector<std::tuple<std::string, std::string>> VideoDriver::GetPictureNames()
    {
        return {{"main", "BGR"}};
    }
}