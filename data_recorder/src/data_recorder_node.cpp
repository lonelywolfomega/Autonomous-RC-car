#include <chrono>
#include <string>
#include <fstream>
#include <csignal>
#include <ros/ros.h>
#include <cv_bridge/cv_bridge.h>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <sensor_msgs/Joy.h>
#include <sensor_msgs/CompressedImage.h>

using namespace std::chrono;

std::ofstream record_file;

/** DataRecorder Class
	Saves images from a camera node and command values
	from joystick axes.
*/
class DataRecorder
{
public:
	DataRecorder();
private:
	ros::NodeHandle nh_;
	ros::Subscriber joy_sub_, camera_sub_;
	int linear_, angular_;
	float steering_angle_, throttle_;
	float dead_zone_;
	cv::Mat image_;
	bool record_;

	void joy_callback(const sensor_msgs::Joy::ConstPtr& joy);
	void camera_callback(const sensor_msgs::CompressedImageConstPtr& img);
	void start_recording(void);
	void toggle_record(void);
};

// custom SIGINT Handler for properly closing the training data file
// before ROS node stops
void signal_handler(int sig)
{
	ROS_INFO("Saving CSV training data.");
	record_file.close();
	ROS_INFO("CSV traing data saved.");

	ros::shutdown();
}

DataRecorder::DataRecorder() :
	linear_(1), angular_(2),
	steering_angle_(0.0), throttle_(0.0),
	dead_zone_(0.0), record_(false)
{
	std::string record_filename = "/home/ubuntu/training_data/training_data.csv";
	std::string raspicam = "raspicam_node/image/compressed";
	std::string joy = "joy";

	ROS_INFO("Setting Up the DataRecorder Node...");

	// create two subsribers to get data from a camera and a joystick
	camera_sub_ = nh_.subscribe<sensor_msgs::CompressedImage>(raspicam, 30,
		&DataRecorder::camera_callback, this);
	joy_sub_ = nh_.subscribe<sensor_msgs::Joy>(joy, 30, &DataRecorder::joy_callback, this);

	record_file.open(record_filename, std::ios_base::app);

	ROS_INFO("Initialization Complete!");
}

void DataRecorder::joy_callback(const sensor_msgs::Joy::ConstPtr& joy)
{
	// store joystick command values
	steering_angle_ = joy->axes[angular_];
	throttle_ = joy->axes[linear_];
	int record_button = joy->buttons[15];
	if (record_button)
		toggle_record();
}

void DataRecorder::camera_callback(const sensor_msgs::CompressedImageConstPtr& img)
{
	// store the image from camera
	cv::Mat cv_img = cv::imdecode(cv::Mat(img->data), CV_LOAD_IMAGE_COLOR);
	cv::resize(cv_img, image_, cv::Size(), 0.25, 0.25);

	if (throttle_ > dead_zone_ && record_)
		start_recording();
}

void DataRecorder::start_recording(void)
{
	// synchronize image data and joystick data using time stamp
	milliseconds ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch());
	std::string img_name = "img_" + std::to_string(ms.count()) + ".jpg";

	imwrite("/home/ubuntu/training_data/" + img_name, image_);
	record_file << img_name + ","
		+ std::to_string(steering_angle_) + ","
		+ std::to_string(throttle_) + "\n";
}

void DataRecorder::toggle_record(void)
{
	// switch the state of the record mode
	record_ = record_ ? false : true;
	std::string state = record_ ? "ON" : "OFF";
	ROS_INFO("Record Mode State: %s", state.c_str());
}

int main(int argc, char** argv)
{
	ros::init(argc, argv, "data_recorder", ros::init_options::NoSigintHandler);
	DataRecorder recorder;

	std::signal(SIGINT, signal_handler);
	ros::spin();

	return 0;
}

