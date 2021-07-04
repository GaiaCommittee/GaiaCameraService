using System;
using StackExchange.Redis;
using System.IO.MemoryMappedFiles;
using System.Runtime.InteropServices;
using System.Security.Claims;
using OpenCvSharp;

namespace Gaia.CameraService
{
    /// <summary>
    /// Reader is bound to a picture in the shared memory block,
    /// provides picture information querying and retrieving functions.
    /// </summary>
    public class PictureReader
    {
        /// <summary>
        /// Stream of the shared memory block.
        /// </summary>
        private readonly MemoryMappedViewStream SharedPictureStream;
        /// <summary>
        /// Name of the picture timestamp configuration item.
        /// </summary>
        private readonly string TimestampName;

        /// <summary>
        /// Width of this picture.
        /// </summary>
        public int Width { get; }
        /// <summary>
        /// Height of this picture.
        /// </summary>
        public int Height { get; }
        /// <summary>
        /// Count of channels of this picture.
        /// </summary>
        public int Channels { get; }
        /// <summary>
        /// Format of the pixel, such as "8U", "32F", etc.
        /// </summary>
        public string Format { get; }

        /// <summary>
        /// Connection to the Redis database.
        /// </summary>
        private readonly IDatabase Database;
        
        /// <summary>
        /// Read picture information and open the shared memory block of this picture.
        /// </summary>
        /// <param name="connection">Connection to the Redis server.</param>
        /// <param name="information_prefix">Name prefix of the picture information items.</param>
        /// <param name="shared_block_name">Name of the shared memory block.</param>
        public PictureReader(IConnectionMultiplexer connection, string information_prefix, string shared_block_name)
        {
            Database = connection.GetDatabase();
            TimestampName = information_prefix + "timestamp";

            Width = (int) Database.StringGet(information_prefix + "width");
            Height = (int) Database.StringGet(information_prefix + "height");
            Channels = (int) Database.StringGet(information_prefix + "channels");
            var option_format = Database.StringGet(information_prefix + "format");
            Format = option_format.HasValue ? option_format.ToString() : "Unknown";
            var shared_picture = MemoryMappedFile.OpenExisting(shared_block_name);
            SharedPictureStream = shared_picture.CreateViewStream();
        }

        /// <summary>
        /// Read the current picture.
        /// </summary>
        /// <returns></returns>
        public Mat Read()
        {
            return Mat.FromStream(SharedPictureStream, ImreadModes.Unchanged);
        }

        /// <summary>
        /// Get the timestamp of the current picture.
        /// </summary>
        /// <returns>Timestamp is the milliseconds since the epoch.</returns>
        public long ReadMillisecondsTimestamp()
        {
            return (long) Database.StringGet(TimestampName);
        }

        /// <summary>
        /// Read the timestamp of the current picture.
        /// </summary>
        /// <returns>Date time of the timestamp.</returns>
        public DateTime ReadTimestamp()
        {
            return DateTime.UnixEpoch + TimeSpan.FromMilliseconds(ReadMillisecondsTimestamp());
        }
    }
}