#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/string.hpp>
#include <vosk_api.h>
#include <portaudio.h>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <algorithm>

const std::string MODEL_PATH = "/home/krawlingkorpse/robotics/vosk-model-en-us-0.22-lgraph";
const int SAMPLE_RATE = 16000;
const int CHUNK_SIZE = 8000;
const std::vector<std::string> WAKE_WORDS = {"aria", "hey aria", "aria please", "area", "orea"};

class ARIAEars : public rclcpp::Node {
private:
    VoskModel *model_ = nullptr;
    VoskRecognizer *recognizer_ = nullptr;
    PaStream *stream_ = nullptr;

    std::thread listen_thread_;
    std::atomic<bool> running_{true};

    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr voice_publisher_;

    std::string detect_wake_word(const std::string & text);
    void listen_loop();

public:
    ARIAEars() : Node("aria_ears") {
        voice_publisher_ = this->create_publisher<std_msgs::msg::String>("/aria/input", 10);

        RCLCPP_INFO(this->get_logger(), "Loading speech model...");
        model_ = vosk_model_new(MODEL_PATH.c_str());
        if (!model_) {
            RCLCPP_ERROR(this->get_logger(), "Failed to load speech model!");
            return;
        }
        recognizer_ = vosk_recognizer_new(model_, SAMPLE_RATE);
        vosk_recognizer_set_words(recognizer_, 1);
        RCLCPP_INFO(this->get_logger(), "Speech model loaded!");

        if (Pa_Initialize() != paNoError) {
            RCLCPP_ERROR(this->get_logger(), "PortAudio init failed!");
            return;
        }
        if (Pa_OpenDefaultStream(&stream_, 1, 0, paInt16, SAMPLE_RATE, CHUNK_SIZE / 2, nullptr, nullptr) != paNoError) {
            RCLCPP_ERROR(this->get_logger(), "Microphone error!");
            return;
        }
        Pa_StartStream(stream_);
        RCLCPP_INFO(this->get_logger(), "Microphone ready!");

        listen_thread_ = std::thread(&ARIAEars::listen_loop, this);
        RCLCPP_INFO(this->get_logger(), "ARIA Ears listening for wake word...");
    }

    ~ARIAEars() {
        running_ = false;
        if (listen_thread_.joinable())
            listen_thread_.join();
        if (stream_) {
            Pa_StopStream(stream_);
            Pa_CloseStream(stream_);
        }
        Pa_Terminate();
        if (recognizer_) vosk_recognizer_free(recognizer_);
        if (model_) vosk_model_free(model_);
    }
};

std::string ARIAEars::detect_wake_word(const std::string & text) {
    std::string lower = text;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    for (const auto & wake_word : WAKE_WORDS) {
        size_t pos = lower.find(wake_word);
        if (pos != std::string::npos) {
            std::string command = lower.substr(pos + wake_word.length());
            size_t start = command.find_first_not_of(' ');
            if (start != std::string::npos)
                command = command.substr(start);
            RCLCPP_INFO(this->get_logger(), "Wake word '%s' found, command: '%s'", wake_word.c_str(), command.c_str());
            return command.empty() ? "hello" : command;
        }
    }
    return "";
}

void ARIAEars::listen_loop() {
    std::vector<short> buffer(CHUNK_SIZE / 2);
    RCLCPP_INFO(this->get_logger(), "Starting listen loop...");

    while (running_ && rclcpp::ok()) {
        Pa_ReadStream(stream_, buffer.data(), CHUNK_SIZE / 2);

        int accepted = vosk_recognizer_accept_waveform(
            recognizer_,
            reinterpret_cast<const char *>(buffer.data()),
            CHUNK_SIZE);

        if (accepted) {
            std::string result = vosk_recognizer_result(recognizer_);

            // extract "text" field from JSON result
            size_t pos = result.find("\"text\" : \"");
            if (pos != std::string::npos) {
                pos += 10;
                size_t end = result.find("\"", pos);
                std::string text = result.substr(pos, end - pos);

                if (!text.empty()) {
                    std::cout << std::endl;
                    RCLCPP_INFO(this->get_logger(), "Heard: \"%s\"", text.c_str());

                    std::string command = detect_wake_word(text);
                    if (!command.empty()) {
                        auto msg = std_msgs::msg::String();
                        msg.data = command;
                        voice_publisher_->publish(msg);
                        RCLCPP_INFO(this->get_logger(), "Published to /aria/input: '%s'", command.c_str());
                    } else {
                        RCLCPP_INFO(this->get_logger(), "No wake word detected");
                    }
                }
            }
            vosk_recognizer_reset(recognizer_);
        } else {
            std::string partial = vosk_recognizer_partial_result(recognizer_);
            size_t pos = partial.find("\"partial\" : \"");
            if (pos != std::string::npos) {
                pos += 13;
                size_t end = partial.find("\"", pos);
                std::string partial_text = partial.substr(pos, end - pos);
                if (!partial_text.empty())
                    std::cout << "Hearing: " << partial_text << "\r" << std::flush;
            }
        }
    }
}

int main(int argc, char *argv[]) {
    rclcpp::init(argc, argv);
    auto ears = std::make_shared<ARIAEars>();

    std::cout << "ARIA Ears Online!" << std::endl;
    std::cout << std::string(50, '=') << std::endl;
    std::cout << "Say: 'Hey ARIA [command]' or 'ARIA [command]'" << std::endl;
    std::cout << std::string(50, '=') << std::endl;

    rclcpp::spin(ears);
    rclcpp::shutdown();
    return 0;
}