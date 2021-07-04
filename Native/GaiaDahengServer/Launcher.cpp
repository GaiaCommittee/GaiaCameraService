#include <GaiaCameraServer/GaiaCameraServer.hpp>
#include "DahengCamera.hpp"

#include <GxIAPI.h>

int main(int arguments_count, char** arguments)
{
    GXInitLib();

    Gaia::CameraService::LaunchServer<Gaia::CameraService::DahengCamera>(arguments_count, arguments);

    GXCloseLib();

    return 0;
}