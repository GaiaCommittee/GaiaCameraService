#pragma once

#include <boost/program_options.hpp>
#include <string>
#include <iostream>
#include "CameraServer.hpp"

namespace Gaia::CameraService
{
    /**
     * @brief Launch the camera server with the given type of camera driver.
     * @tparam CameraClass Type of camera driver.
     * @tparam ArgumentTypes Type of camera driver constructor arguments.
     * @param constructor_arguments Arguments to pass to camera driver constructor.
     * @details
     *  This function will block until the server receive a shutdown command and exit normally.
     */
    template <typename CameraClass, typename... ArgumentTypes>
    void LaunchServer(int command_line_counts, char** command_line, ArgumentTypes... constructor_arguments)
    {
        using namespace boost::program_options;

        options_description options("Options");

        options.add_options()
                ("help,?", "show help message.")
                ("host,h", value<std::string>()->default_value("127.0.0.1"),
                 "ip address of the Redis server.")
                ("port,p", value<unsigned int>()->default_value(6379),
                 "port of the Redis server.")
                ("device,d", value<unsigned int>()->default_value(0),
                 "index of the device to open.");
        variables_map variables;
        store(parse_command_line(command_line_counts, command_line, options), variables);
        notify(variables);

        if (variables.count("help"))
        {
            std::cout << options << std::endl;
            return;
        }

        auto option_host = variables["host"].as<std::string>();
        auto option_port = variables["port"].as<unsigned int>();
        auto option_device = variables["device"].as<unsigned int>();

        bool crashed;
        do
        {
            try
            {
                crashed = false;
                std::cout << "Launching camera server on device index " << option_device
                    << ", with Redis server on " << option_host << ":" << option_port << "..." << std::endl;

                Gaia::CameraService::CameraServer server(
                        std::make_unique<CameraClass>(constructor_arguments...),
                                option_device,
                                option_port, option_host);
                std::cout << "Camera server online." << std::endl;
                server.Launch();
                std::cout << "Camera server stopped." << std::endl;
            }catch (std::exception& error)
            {
                crashed = true;
                std::cout << "Camera server crashed, exception:" << std::endl;
                std::cout << error.what() << std::endl;
                std::cout << "Camera server will restart in 1 second." << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        } while (crashed);
    }
}