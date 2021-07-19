#include <GaiaCameraServer/GaiaCameraServer.hpp>
#include "VideoDriver.hpp"

#include <GxIAPI.h>

int main(int arguments_count, char** arguments)
{
    Gaia::CameraService::LaunchServer<Gaia::CameraService::VideoDriver>(arguments_count, arguments);

    return 0;
}