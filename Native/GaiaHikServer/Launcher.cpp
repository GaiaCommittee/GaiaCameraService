#include <GaiaCameraServer/GaiaCameraServer.hpp>
#include "HikCamera.hpp"

#include <GxIAPI.h>

int main(int arguments_count, char** arguments)
{
    Gaia::CameraService::LaunchServer<Gaia::CameraService::HikCamera>(arguments_count, arguments);

    return 0;
}