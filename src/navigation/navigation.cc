//========================================================================
//  This software is free: you can redistribute it and/or modify
//  it under the terms of the GNU Lesser General Public License Version 3,
//  as published by the Free Software Foundation.
//
//  This software is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public License
//  Version 3 in the file COPYING that came with this distribution.
//  If not, see <http://www.gnu.org/licenses/>.
//========================================================================
/*!
\file    navigation.cc
\brief   Starter code for navigation.
\author  Joydeep Biswas, (C) 2019
*/
//========================================================================

#include "gflags/gflags.h"
#include "eigen3/Eigen/Dense"
#include "eigen3/Eigen/Geometry"
#include "amrl_msgs/AckermannCurvatureDriveMsg.h"
#include "amrl_msgs/Pose2Df.h"
#include "amrl_msgs/VisualizationMsg.h"
#include "glog/logging.h"
#include "ros/ros.h"
#include "shared/math/math_util.h"
#include "shared/util/timer.h"
#include "shared/ros/ros_helpers.h"
#include "navigation.h"
#include "visualization/visualization.h"

using Eigen::Vector2f;
using amrl_msgs::AckermannCurvatureDriveMsg;
using amrl_msgs::VisualizationMsg;
using std::string;
using std::vector;

using namespace math_util;
using namespace ros_helpers;

namespace {
ros::Publisher drive_pub_;
ros::Publisher viz_pub_;
VisualizationMsg local_viz_msg_;
VisualizationMsg global_viz_msg_;
AckermannCurvatureDriveMsg drive_msg_;

// Epsilon value for handling limited numerical precision.
const float kEpsilon = 1e-5;
} //namespace

namespace navigation {

Navigation::Navigation(const string& map_file, ros::NodeHandle* n) :
    odom_initialized_(false),
    localization_initialized_(false),
    robot_loc_(0, 0),
    robot_angle_(0),
    robot_vel_(0, 0),
    robot_omega_(0),
    nav_complete_(true),
    nav_goal_loc_(0, 0),
    nav_goal_angle_(0),
    speed(0),
    max_speed(1),
    max_acceleration_magnitude(4),
    max_deceleration_magnitude(4) {
  drive_pub_ = n->advertise<AckermannCurvatureDriveMsg>(
      "ackermann_curvature_drive", 1);
  viz_pub_ = n->advertise<VisualizationMsg>("visualization", 1);
  local_viz_msg_ = visualization::NewVisualizationMessage(
      "base_link", "navigation_local");
  global_viz_msg_ = visualization::NewVisualizationMessage(
      "map", "navigation_global");
  InitRosHeader("base_link", &drive_msg_.header);

}

void Navigation::SetNavGoal(const Vector2f& loc, float angle) {
}

void Navigation::UpdateLocation(const Eigen::Vector2f& loc, float angle) {
  localization_initialized_ = true;
  robot_loc_ = loc;
  robot_angle_ = angle;
}

void Navigation::UpdateOdometry(const Vector2f& loc,
                                float angle,
                                const Vector2f& vel,
                                float ang_vel) {
  robot_omega_ = ang_vel;
  robot_vel_ = vel;
  if (!odom_initialized_) {
    odom_start_angle_ = angle;
    odom_start_loc_ = loc;
    odom_initialized_ = true;
    odom_loc_ = loc;
    odom_angle_ = angle;
    return;
  }
  odom_loc_ = loc;
  odom_angle_ = angle;
}

Eigen::Vector2f Navigation::get_robot_loc()
{
  std::cout << robot_loc_ << std::endl;
  return robot_loc_;
}

float Navigation::get_robot_angle()
{
  return robot_angle_;
}

void Navigation::ObservePointCloud(const vector<Vector2f>& cloud, double time) {
  point_cloud_ = cloud;
}

float Navigation::calculate_distance_to_target(){
  float min_distance = -1000000;
  if (point_cloud_.size() == 0) return -1;

  for (unsigned int i=0; i < point_cloud_.size(); i++)
  {
    float distance = sqrt( pow(point_cloud_[i][0], 2) + pow(point_cloud_[i][1], 2) );

    if ( abs(point_cloud_[i][1]) < 0.001 ) // Checking if i-th point is in straight line or not.
    {
      std::cout << "Information about minimum point: Distance " << distance << " index: " << i << std::endl;
      min_distance = distance;
    }
  }
  return min_distance;
}

void Navigation::updateSpeed(const Eigen::Vector2f& velocity){
  float x=velocity.x();
  float y=velocity.y();
  speed= sqrt(x*x + y*y);
  float distance = calculate_distance_to_target();

  // std::cout<<speed<<std::endl;

  float latency = speed*(1/10);
          std::cout<<"==================="<<std::endl;
  std::cout<<"latency "<<latency<<std::endl;
  distance=distance-latency;
   std::cout<<"distance remaining "<<distance<<std::endl;
  // time_needed_to_stop= (speed*speed)/max_deceleration_magnitude;

  float distance_needed_to_stop= (speed*speed)/(2*max_deceleration_magnitude);
     std::cout<<"distance needed to stop "<<distance_needed_to_stop<<std::endl;
        std::cout<<"==================="<<std::endl;
  // distance_needed_to_cruise=(speed*speed)/(2*max_acceleration_magnitude);

  // safety margin to stop
  // float safety_margin=1.0;


  if (distance_needed_to_stop>=distance){
    // decelerate

  drive_msg_.velocity=speed-max_deceleration_magnitude;
  }
  else if(speed <1){
    drive_msg_.velocity=speed+max_acceleration_magnitude;
  }
  else{
    drive_msg_.velocity=max_speed;
  }

  // otherwise keep going max speed
  
  }

Eigen::Vector2f Navigation::latency_compensation(const float& latency, unsigned int iterations){

  previous_velocities.push_back(robot_vel_);
  previous_locations.push_back(robot_loc_);
  previous_omegas.push_back(robot_omega_);
  previous_speeds.push_back(speed);

  Eigen::Vector2f predicted_location(robot_loc_);

  if (previous_omegas.size()>iterations){
    previous_omegas.pop_front();
    previous_locations.pop_front();
    previous_velocities.pop_front();
    previous_speeds.pop_front();
  }

  if (previous_omegas.size()== iterations){
  for (unsigned int i=0; i < iterations; i++)
  {
    predicted_location.x()= predicted_location.x()+(1/curvature)*cos(previous_velocities[i].x()*latency);
    predicted_location.y()= predicted_location.y()+(1/curvature)*sin(previous_velocities[i].y()*latency);
  }
  // predicted_location=predicted_location*latency;
  std::cout<<"predicted_location"<<predicted_location.x()<<" "<<predicted_location.y()<<std::endl;
  std::cout<<"actual location"<<robot_loc_.x()<<" "<<robot_loc_.y()<<std::endl;
  }
  visualization::DrawCross(predicted_location, 0.4, 0x32a852,global_viz_msg_);
return predicted_location;
}
void Navigation::updatePointCloudToGlobalFrame(){
  float x_p, y_p;
  unsigned int i;
  for (i=0; i < point_cloud_.size(); i++)
  {
    std::cout << point_cloud_[i] << "before" << std::endl;
    x_p = point_cloud_[i][0] * cos( -robot_angle_ ) - point_cloud_[i][1] * sin( -robot_angle_ ) - robot_loc_[0];
    y_p = point_cloud_[i][0] * sin( -robot_angle_ ) + point_cloud_[i][1] * cos( -robot_angle_ ) - robot_loc_[1];
    point_cloud_[i][0] = x_p;
    point_cloud_[i][1] = y_p;
    std::cout << "\n" << point_cloud_[i] << "after" << std::endl;
  }
  std::cout << robot_angle_ << std::endl;
}
void Navigation::DrawCar()
{
  Eigen::Vector2f front_left = Vector2f( car_base_length + (car_length - car_base_length) / 2 + margin, car_width/2 + margin );
  Eigen::Vector2f back_left = Vector2f( - (car_length - car_base_length) / 2 - margin, car_width/2 + margin );

  Eigen::Vector2f front_right = Vector2f( car_base_length + (car_length - car_base_length) / 2 + margin, -car_width/2 - margin );
  Eigen::Vector2f back_right = Vector2f( - (car_length - car_base_length) / 2 - margin, -car_width/2 - margin );

  visualization::DrawLine( front_left, back_left, 0x32a852,local_viz_msg_);
  visualization::DrawLine( front_right, back_right, 0x32a852,local_viz_msg_);

  visualization::DrawLine( front_left, front_right, 0x32a852,local_viz_msg_);
  visualization::DrawLine( back_left, back_right, 0x32a852,local_viz_msg_);
}

void Navigation::Run() {
  // This function gets called 20 times a second to form the control loop.


  // Clear previous visualizations.
  visualization::ClearVisualizationMsg(local_viz_msg_);
  visualization::ClearVisualizationMsg(global_viz_msg_);

  // If odometry has not been initialized, we can't do anything.
  if (!odom_initialized_) return;
  // The control iteration goes here.
  // Feel free to make helper functions to structure the control appropriately.

  // The latest observed point cloud is accessible via "point_cloud_"

  // Eventually, you will have to set the control values to issue drive commands:
  curvature=2;
  drive_msg_.curvature = 2;
  latency_compensation(0.3,6);
  DrawCar();
  std::cout << "Robot variables:" << robot_loc_ << "\n Robot velocity: " << robot_vel_ << robot_angle_ << std::endl;
  // if (point_cloud_set) {std::cout << "Yes, it worked" << point_cloud_.size() << std::endl;
  // }

  // updatePointCloudToGlobalFrame();
  updateSpeed(robot_vel_);
  // std::cout<<robot_loc_.x()<<" "<<robot_loc_.y()<<std::endl;



  // Add timestamps to all messages.
  local_viz_msg_.header.stamp = ros::Time::now();
  global_viz_msg_.header.stamp = ros::Time::now();

  drive_msg_.header.stamp = ros::Time::now();
  // Publish messages.
  viz_pub_.publish(local_viz_msg_);
  viz_pub_.publish(global_viz_msg_);

  drive_pub_.publish(drive_msg_);
}

}  // namespace navigation
