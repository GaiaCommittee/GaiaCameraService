#include <GaiaCameraServer/GaiaCameraServer.hpp>
#include "HikDriver.hpp"

#include <GxIAPI.h>

int main(int arguments_count, char** arguments)
{
    Gaia::CameraService::LaunchServer<Gaia::CameraService::HikDriver>(arguments_count, arguments);

    return 0;
}