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

#==============================
# Project Modules
#==============================

add_subdirectory("GaiaCameraClient")
add_subdirectory("GaiaDahengServer")

if (WITH_TEST)
#    add_subdirectory("GaiaLogClientTest")
endif()
