#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/string.hpp>
#include <iostream>
#include <string>
#include <cstdlib>

class ARIATerminal : public rclcpp::Node
{
public:
    ARIATerminal() : Node("aria_terminal") {
        input_publisher_ = this->create_publisher<std_msgs::msg::String>("/aria/input", 10);

        RCLCPP_INFO(this->get_logger(), "ARIA Terminal ready! Type messages to send to ARIA.");
        RCLCPP_INFO(this->get_logger(), "Commands: 'quit', 'exit', or Ctrl+C to stop");
    }

    void send_message(const std::string & text) {
        auto msg = std_msgs::msg::String();
        msg.data = text;
        input_publisher_->publish(msg);
        RCLCPP_INFO(this->get_logger(), "Sent to ARIA: \"%s\"", text.c_str());
    }

    ~ARIATerminal() {
        RCLCPP_INFO(this->get_logger(), "ARIATerminal destroyed");
    }

private:
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr input_publisher_;
};

int main(int argc, char * argv[])
{
    rclcpp::init(argc, argv);
    auto terminal = std::make_shared<ARIATerminal>();

    std::cout << "ARIA Terminal Interface" << std::endl;
    std::cout << std::string(40, '=') << std::endl;
    std::cout << "Type your messages below to send to ARIA" << std::endl;
    std::cout << "Commands: 'quit', 'exit', or Ctrl+C to stop" << std::endl;
    std::cout << std::string(40, '=') << std::endl;

    std::string user_input;

    while (rclcpp::ok()) {
        std::cout << "You: ";
        if (!std::getline(std::cin, user_input)) {
            std::cout << "\nGoodbye!" << std::endl;
            break;
        }

        if (user_input == "quit" || user_input == "exit" || user_input == "bye") {
            std::cout << "Goodbye!" << std::endl;
            break;
        }

        if (!user_input.empty()) {
            terminal->send_message(user_input);
        }

        rclcpp::spin_some(terminal);
    }

    rclcpp::shutdown();
    // std::cout << terminal << std::endl;
    return 0;
}

