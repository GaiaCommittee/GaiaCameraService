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
             "type of the device to open.")
            ("index,i", value<unsigned int>()->implicit_value(0),
             "index of the device to open.")
            ("picture,p", value<std::string>()->implicit_value("main"),
             "name of the picture to show.")
            ("width,w", value<unsigned int>(), "width of the window to resize")
            ("height,h", value<unsigned int>(), "height of the window to resize");
    variables_map variables;
    store(parse_command_line(arguments_count, arguments, options), variables);
    notify(variables);

    if (variables.count("help"))
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

    std::string picture_name;
    if (!variables.count("picture"))
    {
        std::cout << "Camera connected, pictures:" << std::endl;
        for (const auto& name : client.GetPictures())
        {
            std::cout << name << std::endl;
        }
        std::cout << "Input picture name: ";
        std::cin >> picture_name;
    }
    else
    {
        picture_name = variables["picture"].as<std::string>();
    }

    auto reader = client.GetReader(picture_name);

    bool resize = false;
    unsigned int resize_width = 0;
    unsigned int resize_height = 0;
    if (variables.count("width") && variables.count("height"))
    {
        resize_width = variables["width"].as<unsigned int>();
        resize_height = variables["height"].as<unsigned int>();
        resize = true;
    }

    auto title_name = camera_type + "-" + std::to_string(camera_index) + ": " + picture_name;

    // Max 60 FPS
    while (cv::waitKey(15) != 27)
    {
        auto picture = reader.Read();
        if (resize)
        {
            cv::resize(picture, picture,
                       cv::Size(static_cast<int>(resize_width), static_cast<int>(resize_height)));
        }
        cv::imshow(title_name, picture);
    }
}