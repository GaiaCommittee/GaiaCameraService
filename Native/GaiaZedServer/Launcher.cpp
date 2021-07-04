#include <GaiaCameraServer/GaiaCameraServer.hpp>
#include "ZedDriver.hpp"

int main(int arguments_count, char** arguments)
{
    Gaia::CameraService::LaunchServer<Gaia::CameraService::ZedDriver>(arguments_count, arguments);

    return 0;
}