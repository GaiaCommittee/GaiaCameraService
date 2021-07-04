#include "CameraServer.hpp"

#include <thread>

namespace Gaia::CameraService
{
    /// Connect to the Redis server.
    CameraServer::CameraServer(std::unique_ptr<CameraDriverInterface>&& camera_driver,
                               unsigned int device_index,
                               unsigned int port, const std::string &ip) :
       CameraDriver(std::move(camera_driver))
    {
        if (!CameraDriver) throw std::runtime_error("Null camera driver.");
        CameraDriver->Initialize(device_index, this);

        sw::redis::ConnectionOptions connection_options;
        connection_options.socket_timeout = std::chrono::milliseconds(100);
        connection_options.host = ip;
        connection_options.port = static_cast<int>(port);
        connection_options.type = sw::redis::ConnectionType::TCP;
        Connection = std::make_shared<sw::redis::Redis>(connection_options);
        Subscriber = std::make_unique<sw::redis::Subscriber>(Connection->subscriber());
        Subscriber->subscribe("cameras/" + CameraDriver->DeviceName + "/command");
        Subscriber->on_message([this](const std::string& channel, const std::string& value){
           this->HandleCommand(value);
        });

        Logger = std::make_unique<LogService::LogClient>(Connection);
        Logger->Author = CameraDriver->DeviceName;

        Configurator = std::make_unique<ConfigurationService::ConfigurationClient>(CameraDriver->DeviceName, Connection);

        NameResolver = std::make_unique<NameService::NameClient>(Connection);
        NameResolver->RegisterName(CameraDriver->DeviceName);
    }

    /// Stop the updater if it's still running.
    CameraServer::~CameraServer()
    {
        // Close camera device.
        if (CameraDriver)
        {
            CameraDriver->Close();
        }
    }

    /// Launch the Daheng camera server.
    void CameraServer::Launch()
    {
        if (LifeFlag)
        {
            Logger->RecordError("Launch() is invoked when life flag is true.");
            return;
        }

        LifeFlag = true;

        // Open camera.
        Logger->RecordMilestone("Try to open the camera " + CameraDriver->DeviceName + "...");
        CameraDriver->Open();
        Logger->RecordMilestone("Camera " + CameraDriver->DeviceName + " opened.");

        // Register camera.
        Connection->sadd("cameras", CameraDriver->DeviceName);

        // Configure camera.
        Logger->RecordMilestone("Try to configure camera...");
        auto exposure = Configurator->Get<unsigned int>("Exposure");
        if (exposure) CameraDriver->SetExposure(*exposure);
        auto gain = Configurator->Get<double>("Gain");
        if (gain) CameraDriver->SetGain(*gain);
        auto balance_red = Configurator->Get<double>("WhiteBalanceRed");
        if (balance_red) CameraDriver->SetWhiteBalanceRed(*balance_red);
        auto balance_green = Configurator->Get<double>("WhiteBalanceGreen");
        if (balance_green) CameraDriver->SetWhiteBalanceGreen(*balance_green);
        auto balance_blue = Configurator->Get<double>("WhiteBalanceBlue");
        if (balance_blue) CameraDriver->SetWhiteBalanceBlue(*balance_blue);
        Logger->RecordMilestone("Camera configured.");

        auto pictures_list_key ="cameras/" + CameraDriver->DeviceName + "/pictures";
        const auto& picture_names = CameraDriver->GetPictureNames();
        const auto& picture_formats = CameraDriver->GetPictureFormats();
        const auto& picture_channels = CameraDriver->GetPictureChannels();

        if (picture_formats.size() < picture_names.size() || picture_channels.size() < picture_names.size())
        {
            GetLogger()->RecordError("Information of pictures is incomplete, registration failed.");
            throw std::runtime_error("Information of pictures is incomplete, registration failed.");
        }

        for (auto picture_index = 0; picture_index < CameraDriver->GetPictureNames().size(); ++picture_index)
        {
            // Register pictures.
            Connection->sadd(pictures_list_key, picture_names[picture_index]);
            auto picture_information_prefix = "cameras/" + CameraDriver->DeviceName +
                    "/pictures/" + picture_names[picture_index];

            Connection->set(picture_information_prefix + "/width",
                            std::to_string(CameraDriver->GetPictureWidth()));
            Connection->set(picture_information_prefix + "/height",
                            std::to_string(CameraDriver->GetPictureHeight()));
            Connection->set(picture_information_prefix +  "/channels",
                            std::to_string(picture_channels[picture_index].size()));
            Connection->set(picture_information_prefix + "/format",
                            picture_formats[picture_index]);
        }
        Logger->RecordMilestone("Picture information registered.");

        // Enter main loop.
        auto last_status_update_time = std::chrono::system_clock::now();
        while (LifeFlag)
        {
            // Time out time is 10ms.
            try
            {
                Subscriber->consume();
            }catch (sw::redis::TimeoutError& error)
            {}

            auto current_time = std::chrono::system_clock::now();
            auto elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                    current_time - last_status_update_time).count();
            if (elapsed_time >= 1000)
            {
                Connection->set("cameras/" + CameraDriver->DeviceName + "/status/fps",
                                std::to_string(CameraDriver->AcquireReceivedFrameCount()));
                NameResolver->Update();
                last_status_update_time = current_time;
            }
        }

        // Unregister camera.
        Connection->srem("cameras", CameraDriver->DeviceName);

        // Unregister pictures.
        Connection->del("cameras/" + CameraDriver->DeviceName + "/pictures");

        for (const auto& picture_name : CameraDriver->GetPictureNames())
        {
            Connection->del("cameras/" + CameraDriver->DeviceName + "/pictures/" + picture_name + "/width");
            Connection->del("cameras/" + CameraDriver->DeviceName + "/pictures/" + picture_name + "/height");
            Connection->del("cameras/" + CameraDriver->DeviceName + "/pictures/" + picture_name + "/channels");
            Connection->del("cameras/" + CameraDriver->DeviceName + "/pictures/" + picture_name + "/format");
        }
        Logger->RecordMilestone("Picture information unregistered.");

        // Close camera.
        CameraDriver->Close();
        Logger->RecordMilestone("Camera closed.");
    }

    /// Handle command.
    void CameraServer::HandleCommand(const std::string &command)
    {
        if (command == "shutdown") {
            Logger->RecordMilestone("Shutdown command received.");
            CameraDriver->Close();
            LifeFlag = false;
        } else if (command == "save") {
            Configurator->Apply();
            Logger->RecordMessage("Configuration saved.");
        } else if (command == "update_exposure") {
            auto exposure = Configurator->Get<unsigned int>("Exposure");
            if (exposure)
            {
                if (CameraDriver->SetExposure(*exposure))
                {
                    Logger->RecordMessage("Exposure is updated to " + std::to_string(*exposure));
                }
                else
                {
                    Logger->RecordError("Failed to update exposure to " + std::to_string(*exposure) + ".");
                }
            }
            else
            {
                Logger->RecordWarning("Exposure is required to update, but its configuration value is missing.");
            }
        } else if (command == "update_gain") {
            auto gain = Configurator->Get<double>("Gain");
            if (gain)
            {
                if (CameraDriver->SetGain(*gain))
                {
                    Logger->RecordMessage("Gain is updated to " + std::to_string(*gain));
                }
                else
                {
                    Logger->RecordError("Failed to update gain to " + std::to_string(*gain) + ".");
                }
            }
            else
            {
                Logger->RecordWarning("Gain is required to update, but its configuration value is missing.");
            }
        } else if (command == "update_white_balance") {
            auto balance_red = Configurator->Get<double>("WhiteBalanceRed");
            if (balance_red)
            {
                if (CameraDriver->SetWhiteBalanceRed(*balance_red))
                {
                    Logger->RecordMessage("White balance red channel is updated to " + std::to_string(*balance_red));
                }
                else
                {
                    Logger->RecordError("Failed to update white balance red channel to " +
                        std::to_string(*balance_red) + ".");
                }
            }
            auto balance_green = Configurator->Get<double>("WhiteBalanceGreen");
            if (balance_green)
            {
                if (CameraDriver->SetWhiteBalanceGreen(*balance_green))
                {
                    Logger->RecordMessage("White balance green channel is updated to " + std::to_string(*balance_green));
                }
                else
                {
                    Logger->RecordError("Failed to update white balance green channel to " +
                                        std::to_string(*balance_green) + ".");
                }
            }
            auto balance_blue = Configurator->Get<double>("WhiteBalanceBlue");
            if (balance_blue)
            {
                if (CameraDriver->SetWhiteBalanceBlue(*balance_blue))
                {
                    Logger->RecordMessage("White balance blue channel is updated to " + std::to_string(*balance_blue));
                }
                else
                {
                    Logger->RecordError("Failed to update white balance blue channel to " +
                                        std::to_string(*balance_blue) + ".");
                }
            }
        } else if (command == "auto_exposure") {
            if (CameraDriver->AutoAdjustExposure())
            {
                auto exposure = CameraDriver->GetExposure();
                Configurator->Set("Exposure", exposure);
                Logger->RecordMessage("Exposure is auto adjusted to " + std::to_string(exposure));
            }
            else
            {
                Logger->RecordError("Failed to auto adjust exposure.");
            }
        } else if (command == "auto_gain") {
            if (CameraDriver->AutoAdjustGain())
            {
                auto gain = CameraDriver->GetGain();
                Configurator->Set("Gain", gain);
                Logger->RecordMessage("Gain is auto adjusted to " + std::to_string(gain));
            }
            else
            {
                Logger->RecordError("Failed to auto adjust gain.");
            }
        }
        else if (command == "auto_white_balance")
        {
            if (CameraDriver->AutoAdjustWhiteBalance())
            {
                auto balance_red = CameraDriver->GetWhiteBalanceRed();
                Configurator->Set("WhiteBalanceRed", balance_red);
                Logger->RecordMessage("White balance red channel is auto adjusted to " +
                    std::to_string(balance_red));

                auto balance_blue = CameraDriver->GetWhiteBalanceBlue();
                Configurator->Set("WhiteBalanceBlue", balance_blue);
                Logger->RecordMessage("White balance blue channel is auto adjusted to " +
                    std::to_string(balance_blue));

                auto balance_green = CameraDriver->GetWhiteBalanceGreen();
                Configurator->Set("WhiteBalanceGreen", balance_green);
                Logger->RecordMessage("White balance green channel is auto adjusted to " +
                                      std::to_string(balance_green));
            }
            else
            {
                Logger->RecordError("Failed to auto adjust white balance.");
            }
        }
        else
        {
            Logger->RecordWarning("Unknown command '" + command + "' received.");
        }
    }

    /// Update the timestamp of the target picture.
    void CameraServer::UpdatePictureTimestamp(const std::string &picture_name)
    {
        // A long integer.
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
        Connection->set("cameras/" + CameraDriver->DeviceName + "/pictures/" + picture_name + "/timestamp",
                        std::to_string(timestamp));
    }
}