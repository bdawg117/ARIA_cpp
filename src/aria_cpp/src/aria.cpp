#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/string.hpp>
#include <ament_index_cpp/get_package_share_directory.hpp>
#include <llama.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <regex>
#include <cstdlib>

class SmartARIA : public rclcpp::Node {
private:
    std::string model_path = "/home/krawlingkorpse/robotics/llama.cpp/models/Phi-4-mini-instruct.BF16.gguf";

    llama_model *model_ = nullptr;
    llama_context *ctx_ = nullptr;

    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr chatter_publisher_;
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr tts_publisher_;
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr mood_publisher_;
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr input_subscription_;

    std::string load_system_prompt();
    std::string chat_with_aria(const std::string & user_message);
    void speak(const std::string & text, const std::string & mood = "normal");
    std::string execute_ros_command(const std::string & command);
    void send_ros_message(const std::string & text);
    void parse_and_execute_commands(const std::string & aria_response);
    void process_message(const std::string & user_input);
    void input_callback(const std_msgs::msg::String & msg);

public:
    SmartARIA() : Node("smart_aria") {
        chatter_publisher_ = this->create_publisher<std_msgs::msg::String>("chatter", 10);
        tts_publisher_ = this->create_publisher<std_msgs::msg::String>("/aria/speak", 10);
        mood_publisher_ = this->create_publisher<std_msgs::msg::String>("/aria/mood", 10);

        input_subscription_ = this->create_subscription<std_msgs::msg::String>(
            "/aria/input", 10,
            std::bind(&SmartARIA::input_callback, this, std::placeholders::_1));

        std::cout << "Loading ARIA's brain (Phi4.5)..." << std::endl;

        llama_model_params model_params = llama_model_default_params();
        model_params.n_gpu_layers = 50;
        model_ = llama_model_load_from_file(model_path.c_str(), model_params);
        if (!model_) {
            RCLCPP_ERROR(this->get_logger(), "Failed to load model!");
            return;
        }

        llama_context_params ctx_params = llama_context_default_params();
        ctx_params.n_ctx = 8192;
        ctx_ = llama_init_from_model(model_, ctx_params);
        if (!ctx_) {
            RCLCPP_ERROR(this->get_logger(), "Failed to create context!");
            return;
        }

        std::cout << "ARIA is ready!" << std::endl;
        RCLCPP_INFO(this->get_logger(), "ARIA listening for input on /aria/input");
    }

    ~SmartARIA() {
        if (ctx_) llama_free(ctx_);
        if (model_) llama_model_free(model_);
    }
};

std::string SmartARIA::load_system_prompt() {
    try {
        std::string share_dir = ament_index_cpp::get_package_share_directory("aria_cpp");
        std::ifstream file(share_dir + "/aria_prompt.txt");
        if (!file.is_open()) throw std::runtime_error("Could not open prompt file");
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    } catch (const std::exception & e) {
        RCLCPP_ERROR(this->get_logger(), "Prompt not found: %s", e.what());
        return "ARIA's instructions were not found. Your only response can be: PROMPT NOT FOUND, PLEASE RELOAD";
    }
}

std::string SmartARIA::chat_with_aria(const std::string & user_message) {
    std::string system_prompt = load_system_prompt();
    std::string prompt = system_prompt + "\n\nUser: " + user_message + "\nARIA: ";

    RCLCPP_INFO(this->get_logger(), "Generating response for: \"%s\"", user_message.c_str());

    // tokenize prompt
    std::vector<llama_token> tokens(prompt.size() + 64);
    int n_tokens = llama_tokenize(
        llama_model_get_vocab(model_),
        prompt.c_str(), prompt.size(),
        tokens.data(), tokens.size(),
        true, true);
    tokens.resize(n_tokens);

    // decode prompt
    llama_batch batch = llama_batch_get_one(tokens.data(), n_tokens);
    llama_decode(ctx_, batch);

    // sample tokens up to max_tokens
    std::string result;
    const int max_tokens = 150;
    llama_sampler *sampler = llama_sampler_chain_init(llama_sampler_chain_default_params());
    llama_sampler_chain_add(sampler, llama_sampler_init_temp(0.7f));
    llama_sampler_chain_add(sampler, llama_sampler_init_dist(42));

    for (int i = 0; i < max_tokens; i++) {
        llama_token token = llama_sampler_sample(sampler, ctx_, -1);
        if (llama_vocab_is_eog(llama_model_get_vocab(model_), token)) break;

        char buf[256];
        int n = llama_token_to_piece(llama_model_get_vocab(model_), token, buf, sizeof(buf), 0, true);
        if (n > 0) {
            std::string piece(buf, n);
            result += piece;
            if (result.find("User:") != std::string::npos) break;
            if (result.size() >= 2 && result.substr(result.size() - 2) == "\n\n") break;
        }

        llama_batch next = llama_batch_get_one(&token, 1);
        llama_decode(ctx_, next);
    }

    llama_sampler_free(sampler);

    // trim trailing newlines
    while (!result.empty() && (result.back() == '\n' || result.back() == ' '))
        result.pop_back();

    RCLCPP_INFO(this->get_logger(), "Generated: \"%s\"", result.c_str());
    return result;
}

void SmartARIA::speak(const std::string & text, const std::string & mood) {
    auto mood_msg = std_msgs::msg::String();
    mood_msg.data = mood;
    mood_publisher_->publish(mood_msg);

    auto tts_msg = std_msgs::msg::String();
    tts_msg.data = text;
    tts_publisher_->publish(tts_msg);

    RCLCPP_INFO(this->get_logger(), "Published TTS: \"%s\"", text.c_str());
}

std::string SmartARIA::execute_ros_command(const std::string & command) {
    std::string result;
    FILE *pipe = popen(command.c_str(), "r");
    if (!pipe) return "Error: failed to run command";
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe))
        result += buffer;
    pclose(pipe);
    if (!result.empty() && result.back() == '\n')
        result.pop_back();
    return result;
}

void SmartARIA::send_ros_message(const std::string & text) {
    auto msg = std_msgs::msg::String();
    msg.data = text;
    chatter_publisher_->publish(msg);
    RCLCPP_INFO(this->get_logger(), "Broadcasted to ROS2: \"%s\"", text.c_str());
}

void SmartARIA::parse_and_execute_commands(const std::string & aria_response) {
    RCLCPP_INFO(this->get_logger(), "Parsing commands in: \"%s\"", aria_response.c_str());

    if (aria_response.find("BROADCAST:") != std::string::npos) {
        std::regex broadcast_re("BROADCAST:\\s*(.+)");
        std::smatch match;
        if (std::regex_search(aria_response, match, broadcast_re))
            send_ros_message(match[1].str());

    } else if (aria_response.find("LIST_TOPICS") != std::string::npos) {
        std::string topics = execute_ros_command("ros2 topic list");
        std::cout << "Active ROS Topics:\n" << topics << std::endl;
        speak("I found some active topics");

    } else if (aria_response.find("LIST_NODES") != std::string::npos) {
        std::string nodes = execute_ros_command("ros2 node list");
        std::cout << "Active ROS Nodes:\n" << nodes << std::endl;
        speak("I found some active nodes");

    } else if (aria_response.find("SYSTEM_INFO") != std::string::npos) {
        std::string info = execute_ros_command("ros2 wtf");
        std::cout << "ROS2 System Info:\n" << info << std::endl;
        speak("System information displayed");

    } else if (aria_response.find("LED_ON") != std::string::npos) {
        execute_ros_command("ros2 topic pub /led std_msgs/msg/Bool 'data: true' --once");
        speak("LED is now on");

    } else if (aria_response.find("LED_OFF") != std::string::npos) {
        execute_ros_command("ros2 topic pub /led std_msgs/msg/Bool 'data: false' --once");
        speak("LED is now off");

    } else if (aria_response.find("SERVO_ANGLE_TO") != std::string::npos) {
        std::regex angle_re("SERVO_ANGLE_TO:\\s*(\\d+)");
        std::smatch match;
        if (std::regex_search(aria_response, match, angle_re)) {
            std::string angle = match[1].str();
            execute_ros_command("ros2 topic pub /servo_angle std_msgs/msg/Int32 'data: " + angle + "' --once");
            speak("Setting servo angle to " + angle + " degrees");
        } else {
            speak("I could not hear the angle");
        }
    }
}

void SmartARIA::process_message(const std::string & user_input) {
    RCLCPP_INFO(this->get_logger(), "Processing: \"%s\"", user_input.c_str());

    std::string aria_response = chat_with_aria(user_input);
    std::cout << "ARIA: " << aria_response << std::endl;

    speak(aria_response);
    parse_and_execute_commands(aria_response);
}

void SmartARIA::input_callback(const std_msgs::msg::String & msg) {
    std::string command = msg.data;
    while (!command.empty() && (command.back() == ' ' || command.back() == '\n'))
        command.pop_back();
    RCLCPP_INFO(this->get_logger(), "Input received: \"%s\"", command.c_str());
    process_message(command);
}

int main(int argc, char *argv[]) {
    rclcpp::init(argc, argv);
    auto aria = std::make_shared<SmartARIA>();

    std::cout << "ARIA is online! Waiting for input..." << std::endl;
    std::cout << "Terminal input  - ros2 run aria_cpp aria_terminal" << std::endl;
    std::cout << "Voice input     - ros2 run aria_cpp aria_ears" << std::endl;
    std::cout << "Voice output    - ros2 run aria_cpp aria_tts" << std::endl;
    std::cout << "\nListening on /aria/input..." << std::endl;

    rclcpp::spin(aria);
    rclcpp::shutdown();
    return 0;
}