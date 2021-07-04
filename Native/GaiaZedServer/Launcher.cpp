#include <GaiaCameraServer/GaiaCameraServer.hpp>
#include "ZedCamera.hpp"

int main(int arguments_count, char** arguments)
{
    Gaia::CameraService::LaunchServer<Gaia::CameraService::ZedCamera>(arguments_count, arguments);

    return 0;
}