#include <algorithm>
#include <cmath>
#include <format>
#include <geometry_msgs/msg/twist.hpp>
#include <iostream>
#include <limits>
#include <ranges>
#include <rclcpp/rclcpp.hpp>
#include <rclcpp/time.hpp>
#include <sensor_msgs/msg/laser_scan.hpp>
#include <string>
#include <vector>

class ARIAWander : public rclcpp::Node {
   private:
	const double backup_duration = 1.5;
	const double wall_follow_interval = 300000.0;

	void laser_callback(const sensor_msgs::msg::LaserScan::SharedPtr msg);
	int angleToIndex(float angle_rad);
	void wall_follow();
	void safe_wander();
	void director();
	void backup();

	// idk if i still want this
	int count = 0;

	// Time stuff
	rclcpp::Time wall_follow_queue_clock = this->now();
	rclcpp::Time start_time;
	
	rclcpp::Time flag_time_check;

	rclcpp::Time wall_follow_time_start;
	rclcpp::Time wall_time_check;

	rclcpp::Time backup_start_time;



	// Pubs n Subs
	rclcpp::Subscription<sensor_msgs::msg::LaserScan>::SharedPtr
	    laser_subscription_;

	rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr cmd_publisher_;

	std::vector<float> sani_laser_data;
	float min_angle_ = 0;
	float max_angle_ = 0;
	float delta_angle_ = 0;
	float range_max_ = 0;
	float range_min_ = 0;

	struct Sectors {
		float front_left_min = std::numeric_limits<float>::max();
		float front_right_min = std::numeric_limits<float>::max();
		float right_min = std::numeric_limits<float>::max();
		float left_min = std::numeric_limits<float>::max();

		float total_front_average_distance = 0;
		float front_left_average_distance = 0;
		float front_right_average_distance = 0;

		float right_average_distance = 0;
		float left_average_distance = 0;

		float total_front_min = 0;
	};
	Sectors calcSector();

	// States
	int backup_dir = 0;
	int wall_follow_dir = 0; // 0 = left, 1 = right
	bool front_clear = true;
	bool isBacking_up = false;
	bool queue_wall_follow = false;

	int wall_follow_flag = 0;

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

	// if (ranges.size() > 0) {
	// 	int cnt = 0;
	// 	if (std::isinf(ranges[cnt])) {
	// 		while (std::isinf(ranges[cnt])) {
	// 			cnt += 1;
	// 		}
	// 		if (ranges[cnt] >= (msg->range_max / 2)) {
	// 			temp = msg->range_max;
	// 		} else {
	// 			temp = msg->range_min;
	// 		}

	// 		int cnt2 = 0;
	// 		while (cnt2 <= cnt) {
	// 			ranges[cnt2] = temp;  // safe — modifying local copy
	// 			cnt2 += 1;
	// 		}
	// 	}

		for (size_t i = 0; i < ranges.size(); i++) {
			if (std::isinf(ranges[i])) {
				sani_laser_data.push_back(msg->range_max);
			} else {
				sani_laser_data.push_back(ranges[i]);
			}
		}
		// lidar is mounted 180° from robot forward, rotate so index 180 = forward
		std::rotate(sani_laser_data.begin(),
		            sani_laser_data.begin() + sani_laser_data.size() / 2,
		            sani_laser_data.end());
	// }
	director();
}

ARIAWander::Sectors ARIAWander::calcSector() {
	Sectors sectors;

	float front_left_distance = 0;
	float front_right_distance = 0;
	float left_distance = 0;
	float right_distance = 0;

	int right_start = angleToIndex(-135 * M_PI / 180.0f);
	int right_end = angleToIndex(-45 * M_PI / 180.0f);
	int front_left_start = angleToIndex(-45 * M_PI / 180.0f);
	int front_middle = angleToIndex(0 * M_PI / 180.0f);
	int front_right_end = angleToIndex(45 * M_PI / 180.0f);
	int left_start = angleToIndex(45 * M_PI / 180.0f);
	int left_end = angleToIndex(135 * M_PI / 180.0f);

	int left_count = 0;
	int right_count = 0;

	int front_left_count = 0;
	int front_right_count = 0;

	for (int i = front_left_start; i < front_middle; i++) {
		front_left_distance += sani_laser_data[i];
		front_left_count += 1;

		sectors.front_left_min = std::min(sectors.front_left_min, sani_laser_data[i]);
	}

	for (int i = front_middle; i < front_right_end; i++) {
		front_right_distance += sani_laser_data[i];
		front_right_count += 1;

		sectors.front_right_min = std::min(sectors.front_right_min, sani_laser_data[i]);
	}

	for (int i = left_start; i < left_end; i++) {
		left_distance += sani_laser_data[i];
		left_count += 1;

		sectors.left_min = std::min(sectors.left_min, sani_laser_data[i]);
	}

	for (int i = right_start; i < right_end; i++) {
		right_distance += sani_laser_data[i];
		right_count += 1;
		sectors.right_min = std::min(sectors.right_min, sani_laser_data[i]);
	}

	sectors.front_left_average_distance = front_left_distance / front_left_count;
	sectors.front_right_average_distance = front_right_distance / front_right_count;

	sectors.right_average_distance = right_distance / right_count;
	sectors.left_average_distance = left_distance / left_count;

	sectors.total_front_average_distance = (sectors.front_left_average_distance + sectors.front_right_average_distance) / 2;
	sectors.total_front_min = std::min(sectors.front_left_min, sectors.front_right_min);

	return sectors;
}

void ARIAWander::director() {
	Sectors sectors = calcSector();

	rclcpp::Duration elapsed_wall_follow = this->now() - wall_follow_queue_clock;


	if (elapsed_wall_follow.seconds() > wall_follow_interval) {
		if (sectors.left_min < sectors.right_min) {
			wall_follow_dir = 0;
		} else {
			wall_follow_dir = 1;
		}


		RCLCPP_INFO(this->get_logger(), "INITIAT WALL FOLLOW");
		wall_follow_queue_clock = this->now();
		wall_follow_flag = true;
	}


	// if (wall_follow_flag == 0 && (flag_time_check.seconds() - flag_start_time.seconds() > 10)) {
	// 	wall_follow_flag = 1;
	// 	// flag_start_time = this->now();
	// 	// flag_time_check.time(0,0);
	// 	wall_follow_time_start = this->now();
	// }
	// ARIAWander::leftwall_follow();
	// if ((wall_time_check.seconds() - wall_follow_time_start.seconds()) > 5) {
	// 	flag_start_time = this->now();
	// 	wall_follow_flag = 0;

	if (isBacking_up) {
		ARIAWander::backup();
		return;
	}

	if (wall_follow_flag) {
		ARIAWander::wall_follow();
		// return;
	}

	if (sectors.total_front_min > 0.5) {
		ARIAWander::safe_wander();
	} else {
		isBacking_up = true;
		backup_start_time = this->now();
		// backup_dir = (sectors.front_left_min > sectors.total_front_min) ? 1 : 0;
		backup_dir = (sectors.left_average_distance > sectors.right_average_distance) ? 1 :0 ;
		ARIAWander::backup();
		RCLCPP_INFO(this->get_logger(), "BACK UP ENGAGED");
	}
}

void ARIAWander::backup() {
	geometry_msgs::msg::Twist cmd;

	rclcpp::Duration elapsed = this->now() - backup_start_time;

	if (elapsed.seconds() < backup_duration) {
		cmd.linear.x = -1;

		if (backup_dir == 1) {
			cmd.angular.z = -0.9;
		} else {
			cmd.angular.z = 0.9;
		}
	} else {
		isBacking_up = false;
		RCLCPP_INFO(this->get_logger(), "DISENGAGED BACKUP");
	}

	cmd_publisher_->publish(cmd);
}

void ARIAWander::wall_follow() {
	// geometry_msgs::msg::Twist cmd;

	if (wall_follow_dir == 0) {
		RCLCPP_INFO(this->get_logger(), "-------------------FOLLOWING LEFT-------------");
	} else {
		RCLCPP_INFO(this->get_logger(), "-------------------FOLLOWING right-------------");
	}

	wall_follow_flag = false;

	// int start = angleToIndex(-135.0f * M_PI / 180.0f);
	// int end = angleToIndex(-45 * M_PI / 180.0f);
	// float left_distance = 0.0;

	// for (int i = start; i < end; i++) {
	// 	left_distance += sani_laser_data[i];
	// 	count += 1;
	// }

	// if (count > 0) left_distance /= count;

	// if (left_distance > 0.5) {
	// 	cmd.angular.z = -0.4;
	// 	cmd.linear.x = 0.5;
	// }
	// if (left_distance < 0.5) {
	// 	cmd.angular.z = 0.4;
	// 	cmd.linear.x = 0.5;
	// }

	// cmd_publisher_->publish(cmd);
}
// for (auto [i, x] : std::views::enumerate(v)) {
//     std::cout << i << ": " << x << "\n";
// }

void ARIAWander::safe_wander() {
	geometry_msgs::msg::Twist cmd;

	RCLCPP_INFO(this->get_logger(), "SAFE WANDER");

	Sectors sectors = calcSector();
	cmd.linear.x = 0.8 * (1 - (sectors.total_front_average_distance/ range_max_));

	if (sectors.total_front_min > 1) {
		cmd.linear.x = 0.8;

	} else {
		cmd.linear.x = 0.8 * (sectors.total_front_min);
	}


	if (std::min(sectors.right_min, sectors.left_min) < 1) {
			float diff =  sectors.right_average_distance - sectors.left_average_distance;
	// float diff = sectors.right_min - sectors.left_min;
	cmd.angular.z = std::clamp(diff * 0.5f, -0.8f, 0.8f);
	} else {
		cmd.angular.z = 0;
	}



	// if (sectors.left_min > sectors.right_min) {
	// 	if (sectors.left_min < 2 ) {
	// 	cmd.angular.z = -1.0 * (1 - (sectors.left_min / range_max_));
	// 	// RCLCPP_INFO(this->get_logger(),"TURNING RIGHT");
	// 	}
	// } else {
	// 	if (sectors.right_min < 2) {
	// 	cmd.angular.z = 1.0 * (1 - (sectors.right_min / range_max_));
	// 	}
	// 	// RCLCPP_INFO(this->get_logger(),"TURNING LEFT");
	// }

	cmd_publisher_->publish(cmd);
}

int main(int argc, char* argv[]) {
	rclcpp::init(argc, argv);
	rclcpp::spin(std::make_shared<ARIAWander>());
	rclcpp::shutdown();
	return 0;
}