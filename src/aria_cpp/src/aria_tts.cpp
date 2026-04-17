#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/string.hpp>
#include <fstream>
#include <iostream>
#include <string>
#include <cstdio>

class ARIA_TTS_Service : public rclcpp::Node
{
private:
    std::string piper_path = "/home/krawlingkorpse/robotics/aria_cc/piper/piper";
    std::string model_path = "/home/krawlingkorpse/robotics/aria_cc/piper/models/en_US-libritts_r-medium.onnx";

    void speak_callback(const std_msgs::msg::String &msg);
    std::string clean_text_for_speech(const std::string &msg);
    void speak(const std::string &msg);

    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr tts_subscription_;

public:
    ARIA_TTS_Service() : Node("aria_tts_service")
    {
        RCLCPP_INFO(this->get_logger(), "ARIA TTS Up and Running.");

        tts_subscription_ = this->create_subscription<std_msgs::msg::String>("/aria/speak", 10, std::bind(&ARIA_TTS_Service::speak_callback, this, std::placeholders::_1));
    }
    ~ARIA_TTS_Service() {}
};

void ARIA_TTS_Service::speak_callback(const std_msgs::msg::String &msg) {
    this->speak(msg.data);
}

std::string ARIA_TTS_Service::clean_text_for_speech(const std::string &msg) {
    size_t pos;
    std::string text = msg;

    // TODO: fix this ugly mess (who am I kidding, this is going to stay here till it becomes a problem)
    while ((pos = text.find("BROADCAST:")) != std::string::npos)
    {
        text.erase(pos, std::string("BROADCAST:").length());
    }

    while ((pos = text.find("LIST_TOPICS")) != std::string::npos)
    {
        text.erase(pos, std::string("LIST_TOPICS").length());
    }

    while ((pos = text.find("LIST_NODES")) != std::string::npos)
    {
        text.erase(pos, std::string("LIST_NODES").length());
    }

    while ((pos = text.find("SYSTEM_INFO")) != std::string::npos)
    {
        text.erase(pos, std::string("SYSTEM_INFO").length());
    }

    while ((pos = text.find("\n")) != std::string::npos)
    {
        text.replace(pos, std::string("\n").length(), " ");
    }

    while ((pos = text.find("\"")) != std::string::npos)
    {
        text.replace(pos, std::string("\"").length(), "");
    }

    // this is for double spaces
    while ((pos = text.find("  ")) != std::string::npos)
    {
        text.replace(pos, std::string("  ").length(), " ");
    }

    return text;
}

void ARIA_TTS_Service::speak(const std::string &msg)
{

    try {
        std::string clean_text = this->clean_text_for_speech(msg);
        if (clean_text.empty()) {
            return;
        }

        std::ofstream tmp("/tmp/aria_tts_input.txt");
        tmp << clean_text;
        tmp.close();

        // other speaker options are 89 and 42, set to 15 currently
        std::string cmd = piper_path + " --model " + model_path +
                          " --speaker 15 --output_file /tmp/aria_speech.wav" +
                          " < /tmp/aria_tts_input.txt";
        system(cmd.c_str());

        system("aplay /tmp/aria_speech.wav > /dev/null 2>&1 &");

        RCLCPP_INFO(this->get_logger(), "Spoke: %s", clean_text.substr(0, 50).c_str());

        std::remove("/tmp/aria_tts_input.txt");
    } catch (const std::exception &e) {
        RCLCPP_ERROR(this->get_logger(), "TTS FAILED: %s", e.what());
    }
}

int main(int argc, char *argv[]) {
    rclcpp::init(argc, argv);
    auto aria_speek = std::make_shared<ARIA_TTS_Service>();

    std::cout << "Launching ARIA TTS Service" << std::endl;

    rclcpp::spin(aria_speek);
    rclcpp::shutdown();
    return 0;
}