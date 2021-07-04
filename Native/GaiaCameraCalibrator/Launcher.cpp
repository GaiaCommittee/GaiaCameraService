#include <iostream>
#include <boost/program_options.hpp>
#include <GaiaCameraClient/GaiaCameraClient.hpp>

int main(int arguments_count, char** arguments)
{
    using namespace Gaia::CameraService;
    using namespace boost::program_options;

    options_description options("Options");

    options.add_options()
            ("help,?", "show help message.")
            ("device,d", value<std::string>()->implicit_value("daheng"),
                    "type of the device to calibrate.")
            ("index,i", value<unsigned int>()->implicit_value(0),
                    "index of the device to calibrate.")
            ("exposure,e", value<unsigned int>(),
                    "milliseconds exposure value.")
            ("gain,g", value<double>(), "digital gain")
            ("balance_red,R", value<double>(), "balance ratio red channel")
            ("balance_blue,B", value<double>(), "balance ratio blue channel")
            ("balance_green,G", value<double>(), "balance ratio green channel")
            ("auto_exposure,E", "auto adjust exposure.")
            ("auto_gain,A", "auto adjust gain.")
            ("auto_white_balance,W", "auto adjust white balance.");
    variables_map variables;
    store(parse_command_line(arguments_count, arguments, options), variables);
    notify(variables);

    if (variables.empty() || variables.count("help"))
    {
        std::cout << options << std::endl;
        return 0;
    }

    std::string camera_type;
    if (!variables.count("device"))
    {
        std::cout << "Input camera type: ";
        std::cin >> camera_type;
    }
    else
    {
        camera_type = variables["device"].as<std::string>();
    }
    unsigned int camera_index;
    if (!variables.count("index"))
    {
        std::cout << "Input camera index: ";
        std::cin >> camera_index;
    }
    else
    {
        camera_index = variables["index"].as<unsigned int>();
    }

    CameraClient client(camera_type, camera_index);

    if (variables.count("exposure"))
    {
        client.SetExposure(variables["exposure"].as<unsigned int>());
        std::cout << "Exposure adjusted." << std::endl;
    }
    if (variables.count("gain"))
    {
        client.SetGain(variables["gain"].as<double>());
        std::cout << "Gain adjusted." << std::endl;
    }
    if (variables.count("balance_red") || variables.count("balance_blue") || variables.count("balance_green"))
    {
        if (variables.count("balance_red") && variables.count("balance_blue")
            && variables.count("balance_green"))
        {
            client.SetWhiteBalance(variables["balance_red"].as<double>(),
                    variables["balance_green"].as<double>(),
                    variables["balance_blue"].as<double>());
            std::cout << "White balance adjusted." << std::endl;
        }
        else
        {
            std::cout << "To adjust white balance, ratios of all three channels (red, green, blue) must be given."
                << std::endl;
        }
    }
    if (variables.count("auto_exposure"))
    {
        client.AutoAdjustExposure();
        std::cout << "Exposure auto adjusted." << std::endl;
    }
    if (variables.count("auto_gain"))
    {
        client.AutoAdjustGain();
        std::cout << "Gain auto adjusted." << std::endl;
    }
    if (variables.count("auto_white_balance"))
    {
        client.AutoAdjustWhiteBalance();
        std::cout << "White balance auto adjusted." << std::endl;
    }

    return 0;
}