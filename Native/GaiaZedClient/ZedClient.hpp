#pragma once

#include <sl/Camera.hpp>
#include <GaiaCameraClient/GaiaCameraClient.hpp>

namespace Gaia::CameraService
{
    /**
     * @brief Client for camera service, provides function to get the picture in OpenCV matrix format.
     */
    class ZedClient : public CameraClient
    {
    public:
        /// Camera type is "zed" and only one zed is suppored by zed sdk, so index is 0.
        ZedClient();

        /**
         * @brief Get magnetometer data.
         * @return Ambient geomagnetic field for all three physical axes (x, y, z) in μT. Values are uncalibrated.
         */
        sl::float3 GetMagneticField();
        /**
         * @brief Get relative altitude data.
         * @return Relative altitude (altitude variation) from initial camera position.
         */
        float GetRelativeAltitude();
        /**
         * @brief Get linear acceleration applied to the camera.
         * @return Acceleration force applied on all three physical axes (x, y, and z),
         *         including the force of gravity.
         *         Values are corrected from bias, scale and misalignment.
         */
        sl::float3 GetLinearAcceleration();
        /**
         * @brief Get angular velocity data.
         * @return Rate of rotation around each of the three physical axes (x, y, and z).
         *         Values are corrected from bias, scale and misalignment.
         */
        sl::float3 GetAngularVelocity();
        /**
         * @brief Get the orientation angle of this camera.
         * @return X represents pitch, Y represents yaw, Z represents roll,
         *         angles are stored as radians.
         */
        sl::float3 GetOrientation();

        /// Get reader for the right view picture, in color format of BGR.
        [[nodiscard]] inline PictureReader GetLeftViewReader()
        {
            return GetReader("left");
        }
        /// Get reader for the left view picture, in color format of BGR.
        [[nodiscard]] inline PictureReader GetRightViewReader()
        {
            return GetReader("right");
        }
        /**
         * @brief Get reader for the point cloud data.
         * @return A matrix of 4 channels 32 bits float,
         *         first 3 channels are X, Y, and Z, distance to the left camera in unit of centimeter,
         *         last channel is BGRA color channel, every 8 bits belong to a color sector.
         */
        [[nodiscard]] inline PictureReader GetPointCloudReader()
        {
            return GetReader("point_cloud");
        }
    };
}