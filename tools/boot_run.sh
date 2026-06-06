#!/bin/bash

source /opt/ros/humble/setup.bash

source /home/meta/vision_ws/install/local_setup.bash

export ROS_DOMAIN_ID=0

while ! ls /dev/ttyACM* > /dev/null 2>&1; do
    echo "Waiting for the device to be connected..."
    sleep 1
done

echo "Launching auto aim..."
ros2 launch auto_aim_bringup debug.py camera_type:=mindvision profile:=infantry