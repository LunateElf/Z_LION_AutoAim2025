#!/bin/bash
source ~/.bashrc
file_dir=$(realpath $(dirname $0))
vision_ws_dir=$file_dir/..
echo $vision_ws_dir
cd $vision_ws_dir
source /opt/ros/humble/setup.bash
source install/setup.bash
ros2 launch auto_aim_bringup debug.py camera_type:=mindvision profile:=sentry
