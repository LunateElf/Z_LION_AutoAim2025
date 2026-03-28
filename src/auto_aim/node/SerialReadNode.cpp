#include "rclcpp/rclcpp.hpp"
#include "auto_aim_interfaces/msg/serial_read_data.hpp" // 包含自定义消息头文件
#include "../controler/SerialModule/Serial.h"
#include <string>
#include <math.h>
#include <iostream>
#include <cmath>
#include <chrono>
#include <boost/asio.hpp>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <mutex>
#include <queue>
#include <algorithm>
using namespace rm;

class SerialReadNode : public rclcpp::Node {
public:
    SerialReadNode()
        : Node("serial_read_data_node") {
        
        this->debug = declare_parameter("debug",true);
        this->negation_read_yaw = declare_parameter("negation_read_yaw",false);
        this->negation_read_pitch = declare_parameter("negation_read_pitch",false);

        std::vector<SerialBase*> serial_datas;
        Constant* data_head = new Constant(170);
        enemy_color = new Uint_8;
        car_yaw = new Float;
        car_pitch = new Float;
        grade = new Uint_8;
        // Constant* data_tail = new Constant(165);
        CRC16* crc16 = new CRC16;

        serial_datas = {
            data_head,
            enemy_color,
            car_yaw,
            car_pitch,
            grade,
            crc16
        };

        SerialRead__ = std::make_unique<SerialRead>(serial_datas);
        publisher_ = this->create_publisher<auto_aim_interfaces::msg::SerialReadData>("serial_read_data_topic", 10);

        // Use timer polling so rclcpp::spin() can process Ctrl+C promptly.
        read_timer_ = this->create_wall_timer(
            std::chrono::milliseconds(2),
            std::bind(&SerialReadNode::read_once, this));

        RCLCPP_INFO(this->get_logger(), "SerialReadNode is all right!!!");
    };
    ~SerialReadNode() {
        if (fd != -1) {
            close(fd);
            fd = -1;
        }
    }
private:
    bool debug;
    bool negation_read_yaw;
    bool negation_read_pitch;

    Uint_8* enemy_color;
    Float* car_yaw;
    Float* car_pitch;
    Uint_8* grade;
private: // 缓冲区信息和地址
    char buffer[1024];
    int fd = -1;
    std::vector<uint8_t> rx_buffer_;
    size_t fail_count_ = 0;
    std::chrono::steady_clock::time_point next_reopen_time_ = std::chrono::steady_clock::now();
private:
    void read_once()
    {
        if (fd == -1) {
            if (std::chrono::steady_clock::now() >= next_reopen_time_) {
                open_serial();
                if (fd == -1) {
                    next_reopen_time_ = std::chrono::steady_clock::now() + std::chrono::seconds(1);
                }
            }
            return;
        }

        int bytes_read = read(fd, buffer, sizeof(buffer));

        if (bytes_read < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                RCLCPP_ERROR(this->get_logger(), "Serial read error(errno=%d). Reconnecting...", errno);
                close(fd);
                fd = -1;
                next_reopen_time_ = std::chrono::steady_clock::now() + std::chrono::seconds(1);
            }
            return;
        }

        if (bytes_read == 0) {
            return;
        }

        for (int i = 0; i < bytes_read; i++) {
            int data_int = int(buffer[i]);
            if (data_int < 0) data_int = 256 + data_int;
            rx_buffer_.push_back(static_cast<uint8_t>(data_int));
        }

        parse_frames();
    }

    void parse_frames()
    {
        constexpr size_t FRAME_SIZE = 13; // AA + color + yaw + pitch + grade + CRC16

        while (rx_buffer_.size() >= FRAME_SIZE) {
            auto it = std::find(rx_buffer_.begin(), rx_buffer_.end(), static_cast<uint8_t>(0xAA));
            if (it == rx_buffer_.end()) {
                rx_buffer_.clear();
                return;
            }

            if (it != rx_buffer_.begin()) {
                rx_buffer_.erase(rx_buffer_.begin(), it);
            }

            if (rx_buffer_.size() < FRAME_SIZE) {
                return;
            }

            std::vector<uint8_t> frame(rx_buffer_.begin(), rx_buffer_.begin() + FRAME_SIZE);
            if (check_publish(frame)) {
                rx_buffer_.erase(rx_buffer_.begin(), rx_buffer_.begin() + FRAME_SIZE);
                // Compatible with sender that appends trailing 0xA5 after CRC16.
                if (!rx_buffer_.empty() && rx_buffer_.front() == 0xA5) {
                    rx_buffer_.erase(rx_buffer_.begin());
                }
                continue;
            }

            rx_buffer_.erase(rx_buffer_.begin());
        }
    }

    void open_serial()
    {
        fd = -1;
        // 动态扫描 /dev/ttyACM* 设备
        for (int i = 0; i < 10; i++) {
            std::string port = "/dev/ttyACM" + std::to_string(i);
            fd = open(port.c_str(), O_RDWR | O_NOCTTY);
            if (fd != -1) {
                RCLCPP_INFO(this->get_logger(), "Open serial on %s!!!", port.c_str());
                break;
            }
        }
        
        if (fd == -1) {
            RCLCPP_ERROR(this->get_logger(), "Can't open any /dev/ttyACM* device");
            return; // 不再 exit(0)，让外层循环继续重试
        }
        
        struct termios config;
        tcgetattr(fd, &config);
        config.c_cflag = B115200 | CS8 | CLOCAL | CREAD;
        config.c_iflag = IGNPAR;
        config.c_oflag = 0;
        config.c_lflag = 0;
        
        // 设置非阻塞读取或设定超时，避免死锁没法Ctrl+C
        config.c_cc[VMIN] = 0;
        config.c_cc[VTIME] = 1; // 0.1秒超时
        
        tcflush(fd, TCIFLUSH);
        tcsetattr(fd, TCSANOW, &config);
    }

    bool check_publish(const std::vector<uint8_t>& data) {

        if(SerialRead__->check(data)){
            // 发送
            auto message = auto_aim_interfaces::msg::SerialReadData();
            // 填充消息数据
            message.enemy_color = enemy_color->back_data();
            message.car_yaw = this->negation_read_yaw ? -car_yaw->back_data() : car_yaw->back_data();
            message.car_pitch = this->negation_read_pitch ? -car_pitch->back_data() : car_pitch->back_data();
            message.grade = grade->back_data();
            publisher_->publish(message);

            if(debug){
                // 发布消息
                RCLCPP_INFO(this->get_logger(), "Publishing: enemy_color='%d',car_yaw='%f',car_pitch='%f',car_yaw_speed='%f',grade='%d'", 
                    message.enemy_color,
                    message.car_yaw * 180 / 3.1415926,
                    message.car_pitch * 180 / 3.1415926,
                    message.car_yaw_speed,
                    message.grade
                ); // 这里仅作为示例打印第一个元素
            };
            return true;
        }
        else{
            if(debug){
                fail_count_++;
                if (fail_count_ % 200 == 1) {
                    RCLCPP_ERROR(this->get_logger(),"data is fail (count=%zu, size=%zu)", fail_count_, data.size());
                }
            };
            return false;
        }
    }
    rclcpp::Publisher<auto_aim_interfaces::msg::SerialReadData>::SharedPtr publisher_;
    rclcpp::TimerBase::SharedPtr read_timer_;
    std::unique_ptr<SerialRead> SerialRead__;
};

int main(int argc, char * argv[]) {
    rclcpp::init(argc, argv);
    auto node = std::make_shared<SerialReadNode>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}