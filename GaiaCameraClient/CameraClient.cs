using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using StackExchange.Redis;

namespace Gaia.CameraService
{
    /// <summary>
    /// Client for camera service, providing camera basic control and picture retrieving functions.
    /// </summary>
    public class CameraClient
    {
        /// <summary>
        /// Connection to the Redis server.
        /// </summary>
        private readonly IConnectionMultiplexer Connection;
        /// <summary>
        /// Connection to the Redis server.
        /// </summary>
        private readonly IDatabase Database;
        /// <summary>
        /// Subscriber for publish messages to Redis channels.
        /// </summary>
        private readonly ISubscriber Communicator;
        /// <summary>
        /// Generated device name from camera type name and device index.
        /// </summary>
        private readonly string DeviceName;
        /// <summary>
        /// Name prefix of the configuration item. 
        /// </summary>
        private readonly string ConfigurationPrefix;
        /// <summary>
        /// Name of the camera command channel.
        /// </summary>
        private readonly string CommandChannelName;
        
        /// <summary>
        /// Establish a connection to a Redis server on the given address.
        /// </summary>
        /// <param name="type_name">Type name of the camera.</param>
        /// <param name="device_index">Device index of the camera.</param>
        /// <param name="port">Port of the Redis server.</param>
        /// <param name="ip">IP address of the Redis server.</param>
        public CameraClient(string type_name, int device_index, int port = 6379, string ip = "127.0.0.1") : 
            this(type_name, device_index, ConnectionMultiplexer.Connect($"tcp://{ip}:{port.ToString()}"))
        {}
        
        /// <summary>
        /// Reuse the connection to a Redis server.
        /// </summary>
        /// <param name="type_name">Type name of the camera.</param>
        /// <param name="device_index">Device index of the camera.</param>
        /// <param name="connection">Connection to a Redis server.</param>
        public CameraClient(string type_name, int device_index, IConnectionMultiplexer connection)
        {
            Connection = connection;
            DeviceName = $"{type_name}.{device_index.ToString()}";
            ConfigurationPrefix = $"configurations/{DeviceName}/";
            CommandChannelName = $"cameras/{DeviceName}/command";
            Database = connection.GetDatabase();
            Communicator = connection.GetSubscriber();
        }

        /// <summary>
        /// Get the reader of the picture with the given name.
        /// </summary>
        /// <param name="picture_name">Name of the picture to read.</param>
        /// <returns>Reader bound to the picture with the given name.</returns>
        public PictureReader GetReader(string picture_name)
        {
            return new PictureReader(Connection, $"cameras/{DeviceName}/pictures/{picture_name}/",
                $"{DeviceName}.{picture_name}");
        }
        
        /// <summary>
        /// Get all picture names of this camera.
        /// </summary>
        /// <returns>Names.</returns>
        public IEnumerable<string> GetPictureNames()
        {
            return Database.SetMembers($"cameras/{DeviceName}/pictures").Cast<string>();
        }

        /// <summary>
        /// Get current count of frames captured per second.
        /// </summary>
        /// <returns>Frames per second.</returns>
        public int GetFps()
        {
            return (int) Database.StringGet($"camera/{DeviceName}/status/fps");
        }

        /// <summary>
        /// Set the exposure of the camera.
        /// </summary>
        /// <param name="microseconds">Exposure time.</param>
        public void SetExposure(uint microseconds)
        {
            Database.StringSet(ConfigurationPrefix + "Exposure", microseconds);
            Communicator.Publish(CommandChannelName, "update_exposure");
        }

        /// <summary>
        /// Set the gain of the camera.
        /// </summary>
        /// <param name="gain">Digital gain.</param>
        public void SetGain(double gain)
        {
            Database.StringSet(ConfigurationPrefix + "Gain", gain);
            Communicator.Publish(CommandChannelName, "update_gain");
        }

        /// <summary>
        /// Set the white balance value of the camera.
        /// </summary>
        /// <param name="red_ratio">Ratio of red channel.</param>
        /// <param name="green_ratio">Ratio of green channel.</param>
        /// <param name="blue_ratio">Ratio of blue channel.</param>
        public void SetWhiteBalance(double red_ratio, double green_ratio, double blue_ratio)
        {
            Database.StringSet(ConfigurationPrefix + "WhiteBalanceRed", red_ratio);
            Database.StringSet(ConfigurationPrefix + "WhiteBalanceGreen", green_ratio);
            Database.StringSet(ConfigurationPrefix + "WhiteBalanceBlue", blue_ratio);
            Communicator.Publish(CommandChannelName, "update_white_balance");
        }

        /// <summary>
        /// Require the camera to auto adjust the exposure.
        /// </summary>
        public void AutoAdjustExposure()
        {
            Communicator.Publish(CommandChannelName, "auto_exposure");
        }

        /// <summary>
        /// Require the camera to auto adjust the gain.
        /// </summary>
        public void AutoAdjustGain()
        {
            Communicator.Publish(CommandChannelName, "auto_gain");
        }

        /// <summary>
        /// Require the camera to auto adjust the white balance.
        /// </summary>
        public void AutoAdjustWhiteBalance()
        {
            Communicator.Publish(CommandChannelName, "auto_white_balance");
        }
    }
}