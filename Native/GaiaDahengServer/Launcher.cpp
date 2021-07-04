#include <GaiaCameraServer/GaiaCameraServer.hpp>
#include "DahengDriver.hpp"

#include <GxIAPI.h>

int main(int arguments_count, char** arguments)
{
    GXInitLib();

    Gaia::CameraService::LaunchServer<Gaia::CameraService::DahengDriver>(arguments_count, arguments);

    GXCloseLib();

    return 0;
}