#==============================
# Requirements
#==============================

cmake_minimum_required(VERSION 3.10)

#==============================
# Project Settings
#==============================

if (NOT PROJECT_DECLARED)
    project("Gaia Camera Service" LANGUAGES CXX VERSION 0.8)
    set(PROJECT_DECLARED)
endif()

if (NOT PROJECT_SUIT)
    set(PROJECT_SUIT ON)
endif()

#==============================
# Project Modules
#==============================

add_subdirectory("GaiaCameraClient")
add_subdirectory("GaiaCameraServer")

add_subdirectory("GaiaDahengServer")
add_subdirectory("GaiaHikServer")
add_subdirectory("GaiaZedServer")
add_subdirectory("GaiaZedClient")
add_subdirectory("GaiaVideoServer")

add_subdirectory("GaiaCameraViewer")
add_subdirectory("GaiaCameraCalibrator")

if (WITH_TEST)
endif()
