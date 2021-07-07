#pragma once

#include <memory>
#include <list>
#include <vector>
#include <string>
#include <atomic>
#include <sw/redis++/redis++.h>
#include <GaiaLogClient/GaiaLogClient.hpp>
#include <GaiaConfigurationClient/GaiaConfigurationClient.hpp>

namespace Gaia::CameraService
{
    class CameraServer;

    /**
     * @brief Interface for camera controller.
     * @details
     *  After open the camera device, the captured picture will directly be saved in the shared memory block.
     */
    class CameraDriverInterface
    {
        friend class CameraServer;
    private:
        /// Host server.
        CameraServer* Server {nullptr};
        /// Index of the device.
        unsigned int DeviceIndexSource {0};
        /// Name of the device.
        std::string DeviceNameSource;
        /// Type name of the device.
        const std::string DeviceTypeName;

        /**
         * @brief Initialize camera settings.
         * @param device_index Index of the bound device.
         * @param server Pointer to the host camera server.
         */
        void Initialize(unsigned int device_index, CameraServer* server);

    protected:
        /**
         * @brief Constructor which will generate DeviceName.
         * @param type_name Type name of this camera.
         * @param index Index of the bound camera device.
         */
        explicit CameraDriverInterface(std::string type_name);

        /// Update the timestamp of the given picture.
        void UpdatePictureTimestamp(const std::string& picture_name);

        /// Get logger of the host camera server.
        [[nodiscard]] LogService::LogClient* GetLogger() const;
        /// Get configurator of the host camera server.
        [[nodiscard]] ConfigurationService::ConfigurationClient* GetConfigurator() const;
        /// Get connection to the Redis server.
        [[nodiscard]] sw::redis::Redis* GetDatabase() const;

        /// Count of retrieved pictures, used for calculating FPS.
        std::atomic<unsigned long> RetrievedPicturesCount {0};

    public:
        /// Virtual destructor for derived classes.
        virtual ~CameraDriverInterface() = default;

        /// Index of the device.
        const decltype(DeviceIndexSource)& DeviceIndex {DeviceIndexSource};
        /// Name of the device.
        const decltype(DeviceNameSource)& DeviceName {DeviceNameSource};

        /**
         * @brief Get name of the shared memory block for the picture with the given name.
         * @param picture_name Name of the picture.
         * @return Generated picture shared memory block name.
         */
        [[nodiscard]] std::string GetPictureBlockName(const std::string& picture_name);

        /**
         * @brief Get names list of all output pictures.
         * @return List of tuples, first string is picture name, second string is picture color format,
         *         for example, {{"main", "BGR"}, {"vice", "BayerRG"}}
         */
        virtual std::vector<std::tuple<std::string, std::string>> GetPictureNames() = 0;

        /// Open the camera on the given index and start acquisition.
        virtual void Open() = 0;
        /// Close the camera on the given index and stop acquisition.
        virtual void Close() = 0;

        /**
         * @brief Set the exposure of the camera.
         * @param microseconds Exposure time in microseconds.
         */
        virtual bool SetExposure(unsigned int microseconds) = 0;
        /**
         * @brief Get the exposure of the camera.
         * @return Microseconds of the exposure time.
         */
        virtual unsigned int GetExposure() = 0;
        /**
         * @brief Set the digital gain of the camera.
         * @param gain Value of the digital gain.
         */
        virtual bool SetGain(double gain) = 0;
        /// Get the value of the digital gain of the camera.
        virtual double GetGain() = 0;
        /**
         * @brief Set red channel value of the white balance.
         * @param ratio Value of the target channel.
         */
        virtual bool SetWhiteBalanceRed(double ratio) = 0;
        /// Get red channel value of the white balance.
        virtual double GetWhiteBalanceRed() = 0;
        /**
         * @brief Set blue channel value of the white balance.
         * @param ratio Value of the target channel.
         */
        virtual bool SetWhiteBalanceBlue(double ratio) = 0;
        /// Get blue channel value of the white balance.
        virtual double GetWhiteBalanceBlue() = 0;
        /**
         * @brief Set green channel value of the white balance.
         * @param ratio Value of the target channel.
         */
        virtual bool SetWhiteBalanceGreen(double ratio) = 0;
        /// Get green channel value of the white balance.
        virtual double GetWhiteBalanceGreen() = 0;
        /**
         * @brief Auto adjust the exposure for once.
         * @details
         *  The adjusted exposure value will be saved into configuration.
         */
        virtual bool AutoAdjustExposure() { return false;}
        /**
         * @brief Auto adjust the gain for once.
         * @details
         *  The adjusted gain value will be saved into configuration .
         */
        virtual bool AutoAdjustGain() {return false;}
        /**
         * @brief Auto adjust white balance for once.
         * @details
         *  The adjusted white balance value not be saved into configuration.
         */
        virtual bool AutoAdjustWhiteBalance() {return false;}
    };
}