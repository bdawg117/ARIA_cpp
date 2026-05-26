#include <cmath>
#include <geometry_msgs/msg/twist.hpp>
#include <iostream>
#include <ranges>
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/laser_scan.hpp>
#include <string>
#include <vector>
class ARIAWander : public rclcpp::Node {
   private:
	void laser_callback(const sensor_msgs::msg::LaserScan::SharedPtr msg);
	int angleToIndex(float angle_rad);
	void leftwall_follow();
	void safe_wander();

	rclcpp::Subscription<sensor_msgs::msg::LaserScan>::SharedPtr
	    laser_subscription_;

	rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr cmd_publisher_;

	std::vector<float> sani_laser_data;
	float min_angle_ = 0;
	float max_angle_ = 0;
	float delta_angle_ = 0;
	float range_max_ = 0;
	float range_min_ = 0;

   public:
	ARIAWander() : Node("aria_wander") {
		RCLCPP_INFO(this->get_logger(), "ARIA safe wander activated");

		laser_subscription_ =
		    this->create_subscription<sensor_msgs::msg::LaserScan>(
		        "/scan", 10,
		        std::bind(&ARIAWander::laser_callback, this,
		                  std::placeholders::_1));

		cmd_publisher_ =
		    this->create_publisher<geometry_msgs::msg::Twist>("/cmd_vel", 10);
	}
};
int ARIAWander::angleToIndex(float angle_rad) {
	return (int)((angle_rad - min_angle_) / delta_angle_);
}
void ARIAWander::laser_callback(const sensor_msgs::msg::LaserScan::SharedPtr msg) {
	float temp = 0.0;
	sani_laser_data.clear();
	std::vector<float> ranges = msg->ranges;  // local copy
	min_angle_ = msg->angle_min;
	max_angle_ = msg->angle_max;
	delta_angle_ = msg->angle_increment;
	range_max_ = msg->range_max;
	range_min_ = msg->range_min;

	if (ranges.size() > 0) {
		int cnt = 0;
		if (std::isinf(ranges[cnt])) {
			while (std::isinf(ranges[cnt])) {
				cnt += 1;
			}
			if (ranges[cnt] >= (msg->range_max / 2)) {
				temp = msg->range_max;
			} else {
				temp = msg->range_min;
			}

			int cnt2 = 0;
			while (cnt2 <= cnt) {
				ranges[cnt2] = temp;  // safe — modifying local copy
				cnt2 += 1;
			}
		}

		for (size_t i = 0; i < ranges.size(); i++) {
			if (std::isinf(ranges[i])) {
				if (i > 0 && ranges[i - 1] >= (msg->range_max / 2)) {
					sani_laser_data.push_back(msg->range_min);
				} else {
					sani_laser_data.push_back(msg->range_max);
				}
			} else {
				sani_laser_data.push_back(ranges[i]);
			}
		}
	}
	safe_wander();
}

void ARIAWander::leftwall_follow() {
	geometry_msgs::msg::Twist cmd;
	int count = 0;
	int start = angleToIndex(-135.0f * M_PI / 180.0f);
	int end = angleToIndex(-45 * M_PI / 180.0f);
	float left_distance = 0.0;

	for (int i = start; i < end; i++) {
		left_distance += sani_laser_data[i];
		count += 1;
	}

	if (count > 0) left_distance /= count;

	if (left_distance > 0.3) {
		cmd.angular.z = 0.4;
		cmd.linear.x = 0.5;
	}
	if (left_distance < 0.3) {
		cmd.angular.z = -0.4;
		cmd.linear.x = 0.5;
	}

	cmd_publisher_->publish(cmd);
}
// for (auto [i, x] : std::views::enumerate(v)) {
//     std::cout << i << ": " << x << "\n";
// }

void ARIAWander::safe_wander() {
	geometry_msgs::msg::Twist cmd;

	float front_left_distance = 0;
	float front_right_distance = 0;
	float left_distance = 0;
	float right_distance = 0;
	float front_distance = 0;
	int left_start = angleToIndex(-135 * M_PI / 180.0f);
	int left_end = angleToIndex(-45 * M_PI / 180.0f);
	int front_left_start = angleToIndex(-15 * M_PI / 180.0f);
	int front_middle = angleToIndex(0 * M_PI / 180.0f);
	int front_right_end = angleToIndex(15 * M_PI / 180.0f);
	int right_start = angleToIndex(45 * M_PI / 180.0f);
	int right_end = angleToIndex(135 * M_PI / 180.0f);

	float right_average_distance = 0.0;
	float left_average_distance = 0.0;
	float front_average_distance = 0.0;

	int left_count = 0;
	int right_count = 0;
	int front_count = 0;

	for (int i = left_start; i < left_end; i++) {
		left_distance += sani_laser_data[i];
		left_count += 1;
	}

	for (int i = right_start; i < right_end; i++) {
		right_distance += sani_laser_data[i];
		right_count += 1;
	}

	right_average_distance = right_distance / right_count;
	left_average_distance = left_distance / left_count;

	if (std::min(right_average_distance, left_average_distance) < 0.5) {
		if (right_average_distance > left_average_distance) {
			cmd.angular.z = -0.6 * (1 - left_average_distance);
		} else {
			cmd.angular.z = 0.6 * (1 - right_average_distance);
		}
	}

	for (int i = front_left_start; i < front_right_end; i++) {
		front_distance += sani_laser_data[i];
		front_count += 1;
	}

	front_average_distance = front_distance / front_count;

	if (front_average_distance < 1) {
		cmd.linear.x = 0.7 * (1 - front_average_distance);
		if (front_average_distance < 0.5) {
			cmd.linear.x = -0.2;
		}
		if (left_average_distance > right_average_distance) {
			cmd.angular.z = -1 * (1 - front_average_distance);
		} else {
			cmd.angular.z = 1 * (1 - front_average_distance);
		}
	} else {
		cmd.linear.x = 0.7;
	}

	cmd_publisher_->publish(cmd);
}

int main(int argc, char* argv[]) {
	rclcpp::init(argc, argv);
	rclcpp::spin(std::make_shared<ARIAWander>());
	rclcpp::shutdown();
	return 0;
}