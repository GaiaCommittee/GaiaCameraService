#include <boost/program_options.hpp>
#include <string>
#include <iostream>
#include <thread>
#include <chrono>
#include "DahengServer.hpp"

#include <GxIAPI.h>

int main(int arguments_count, char** arguments)
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
                    "index of the device to open.")
            ("format,f", value<std::string>()->default_value("bayer_rg"),
                    "format of the camera, chosen from 'bayer_rg' and 'bayer_gr'.");
    variables_map variables;
    store(parse_command_line(arguments_count, arguments, options), variables);
    notify(variables);

    if (variables.count("help"))
    {
        std::cout << options << std::endl;
        return 0;
    }

    auto option_host = variables["host"].as<std::string>();
    auto option_port = variables["port"].as<unsigned int>();
    auto option_device = variables["device"].as<unsigned int>();
    auto option_format = variables["format"].as<std::string>();

    GXInitLib();

    bool crashed = false;
    do
    {
        try
        {
            crashed = false;
            std::cout << "Launching Daheng Camera Server on device index " << option_device
                << ", with Redis server on " << option_host << ":" << option_port << "..." << std::endl;
            Gaia::CameraService::DahengServer server;
            server.Launch();
            std::cout << "Daheng Camera Server stopped." << std::endl;
        }catch (std::exception& error)
        {
            crashed = true;
            std::cout << "Daheng Camera Server crashed, exception:" << std::endl;
            std::cout << error.what() << std::endl;
            std::cout << "Daheng Camera Server will restart in 1 second." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    } while (crashed);

    GXCloseLib();

    return 0;
}