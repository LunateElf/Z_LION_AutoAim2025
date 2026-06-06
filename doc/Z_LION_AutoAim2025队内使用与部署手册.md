# Z_LION_AutoAim2025 队内使用与部署手册

本文档面向需要直接使用、调试或部署本视觉项目的队员。

原仓库根目录的 `README.md` 主要介绍算法结构和旧版运行方法；具体参数含义与调参流程请继续参考：

- [Z_LION_AutoAim2025 调参手册](./Z_LION_AutoAim2025调参手册.pdf)

本文档以 fork 仓库的 `tensorrt` 分支和当前 Orin Nano 部署方式为准。

## 1. 项目基线

### 1.1 仓库

- 上游仓库：<https://github.com/dielivelr/Z_LION_AutoAim2025>
- 队内 fork：<https://github.com/LunateElf/Z_LION_AutoAim2025>
- 当前部署分支：`tensorrt`

新车部署和已部署机器更新都应使用 `tensorrt`，不要使用 `master`。fork 的 `master` 当前基本保持上游旧版本，不包含完整的 TensorRT、双相机和开机服务改造。

### 1.2 标准运行命令

完整视觉链路统一由以下 launch 启动：

```bash
ros2 launch auto_aim_bringup debug.py \
  camera_type:=mindvision \
  profile:=infantry
```

两个关键参数：

| 参数 | 可选值 | 作用 |
| --- | --- | --- |
| `camera_type` | `mindvision`、`hik` | 选择迈德威视或海康相机驱动 |
| `profile` | `default`、`hero`、`sentry`、`infantry` | 选择对应机器人的整车参数 |

当前这台 `orin0` 是使用迈德威视相机的步兵：

```text
camera_type=mindvision
profile=infantry
```

其他机器人应根据实际相机和兵种选择组合。例如：

```bash
# 海康相机英雄
ros2 launch auto_aim_bringup debug.py camera_type:=hik profile:=hero

# 迈德威视相机哨兵
ros2 launch auto_aim_bringup debug.py camera_type:=mindvision profile:=sentry
```

## 2. 相比 fork 前上游的主要修改

当前 `tensorrt` 分支相对 fork 前上游大约有 84 个文件变化，新增约 14361 行、删除约 247 行。

### 2.1 相机与整车配置

- 引入 ROS 2 迈德威视相机包及对应 SDK。
- 引入 ROS 2 海康相机包及对应 SDK。
- launch 支持通过 `camera_type` 在两种相机间切换。
- 增加 `hero.yaml`、`sentry.yaml`、`infantry.yaml` 等车型参数。
- 为不同车型增加独立的相机内参文件。
- 支持相机图像旋转、曝光、增益和相机标定文件动态选择。
- 增加步兵、英雄、哨兵的标定数据和坐标系参数。

### 2.2 推理后端

- YOLO 推理后端由 OpenVINO 迁移到 TensorRT/CUDA。
- 优先加载与 ONNX 同名的 TensorRT engine。
- engine 不存在或无法反序列化时，可以从 ONNX 重新构建并缓存 engine。
- 支持 TensorRT 8/10 的接口差异。

配置文件目前仍填写 ONNX 路径：

```yaml
YoloModel:
  yolo_model_path: "/home/meta/vision_ws/src/auto_aim/models/yolo.onnx"
```

程序会自动尝试加载：

```text
/home/meta/vision_ws/src/auto_aim/models/yolo.engine
```

### 2.3 串口与整车通讯

- 串口读取和发送节点支持扫描 `/dev/ttyACM0` 到 `/dev/ttyACM9`。
- 增强串口数据缓存、帧校验和异常重试。
- 各车型配置可独立设置 yaw/pitch 方向取反。
- 启动脚本会等待电控串口设备出现后再启动完整视觉链路。

### 2.4 启动和部署

- 增加赛前环境检查脚本 `scripts/run_auto_aim_match.sh`。
- 增加开机启动脚本 `tools/boot_run.sh`。
- 增加 systemd 安装脚本 `tools/install_boot_run.sh`。
- 正式启动使用 `auto_aim.service`，异常退出后由 systemd 重启。
- 旧 `rc-local.service` 会在安装时被停用和屏蔽，避免重复启动两套视觉。
- `.bashrc` 中保留 `vision_check` 和 `vision_up`，作为调试和备用启动方式。

### 2.5 主要开发时间线

`tensorrt` 分支于 2026 年 3 月 29 日从提交 `385a2cf` 创建。TensorRT 迁移提交本身发生在分支创建前。

分支创建前：

| 日期 | 提交 | 主要内容 |
| --- | --- | --- |
| 2026-03-20 | `d761103` | fork 开发线初始提交，引入迈德威视相机包、SDK、模型和 ROS 2 配置 |
| 2026-03-22 | `16dc2be` | 增加海康/迈德威视切换，以及英雄、哨兵标定和参数 |
| 2026-03-28 | `d43fa8c` | 增加步兵测试版本、多车型参数和串口调整 |
| 2026-03-29 | `385a2cf` | 将 OpenVINO 推理替换为 TensorRT/CUDA |

分支创建后：

| 日期 | 提交 | 主要内容 |
| --- | --- | --- |
| 2026-03-31 | `10540a8` | 构建 TensorRT engine，继续完善步兵融合识别 |
| 2026-04-01 | `8fd76bd` | 增加迈德威视多车型相机配置 |
| 2026-04-01 | `ef9f0d3` | 更新哨兵相机参数 |
| 2026-04-01 | `d22938a` | 完整引入海康相机包和按车型加载标定文件 |
| 2026-04-02 | `97a4c6d` | 增加比赛启动检查脚本并完善双相机处理 |

## 3. 参数文件组织

### 3.1 整车参数

整车参数位于：

```text
src/auto_aim_bringup/config/
```

主要文件：

```text
default.yaml
hero.yaml
sentry.yaml
infantry.yaml
infantry_fusion.yaml
```

每个文件同时包含：

- 相机曝光、增益和旋转。
- 检测和融合识别参数。
- 相机内参、畸变参数和坐标系偏移。
- 滤波、目标规划和射击策略参数。
- 串口方向、关火开关和调试显示选项。

### 3.2 相机标定文件

迈德威视相机：

```text
src/rm_vision_ros2_mindvision_camera/config/
```

海康相机：

```text
src/hik_camera/config/
```

每种相机都有按车型区分的文件：

```text
camera_info_hero.yaml
camera_info_sentry.yaml
camera_info_infantry.yaml
```

更换相机或重新标定时，需要同时检查：

1. 相机包中的 `camera_info_<profile>.yaml`。
2. `src/auto_aim_bringup/config/<profile>.yaml` 中 `Coordinate.intrinsic` 和 `Coordinate.distcoeffs`。

两处参数必须对应同一台相机和同一套标定结果。

## 4. 已部署机器的正式启动

### 4.1 正常比赛启动

已经部署完成的机器人不需要 SSH 登录，也不需要手动执行命令。

正常流程：

```text
机器人给 Orin Nano 供电
  -> systemd 启动 auto_aim.service
  -> tools/boot_run.sh 等待 /dev/ttyACM*
  -> 电控串口连接
  -> 启动相机、串口、检测、解算和调试节点
```

当前服务使用：

```text
camera_type=mindvision
profile=infantry
ROS_DOMAIN_ID=0
```

正式服务只负责加载 ROS 环境、等待串口并启动视觉。它不会自动执行 `vision_check` 中的以下操作：

- 安装 udev 串口规则。
- 设置 `nvpmodel`。
- 执行 `jetson_clocks`。
- 停止 `gdm3`。

这些项目应在新车部署和比赛验收阶段提前处理或确认。

### 4.2 检查服务

```bash
systemctl status auto_aim.service --no-pager -l
```

确认服务是否开机启用：

```bash
systemctl is-enabled auto_aim.service
```

正常应输出：

```text
enabled
```

查看本次开机日志：

```bash
journalctl -u auto_aim.service -b --no-pager
```

持续查看日志：

```bash
journalctl -u auto_aim.service -f
```

如果日志持续显示：

```text
Waiting for the device to be connected...
```

说明没有发现 `/dev/ttyACM*`。优先检查：

```bash
ls -l /dev/ttyACM*
lsusb
```

### 4.3 服务控制

重启视觉：

```bash
sudo systemctl restart auto_aim.service
```

临时停止视觉：

```bash
sudo systemctl stop auto_aim.service
```

重新启动：

```bash
sudo systemctl start auto_aim.service
```

比赛部署中不要同时运行 `auto_aim.service` 和手动 `ros2 launch`，否则两套程序会争抢相机和串口。

### 4.4 确认旧启动方案已经关闭

```bash
systemctl is-enabled rc-local.service
systemctl is-active rc-local.service
```

当前标准状态应为：

```text
masked
inactive
```

## 5. `.bashrc` 备用启动

当前已部署机器的 `~/.bashrc` 中提供：

```bash
vision_check
vision_up
```

### 5.1 使用场景

- systemd 服务暂时不可用。
- 需要 SSH 登录后观察启动输出。
- 需要场外调参。
- 需要将视觉放在 tmux 中运行，断开 SSH 后继续保留进程。

### 5.2 启动步骤

先停止正式服务，释放相机和串口：

```bash
sudo systemctl stop auto_aim.service
```

执行赛前检查：

```bash
vision_check
```

当前 `vision_check` 会：

- 检查工作空间、模型和参数文件。
- 检查 `/dev/ttyACM*` 权限。
- 安装持久化串口 udev 规则。
- 检查相机 USB 设备。
- 设置 Jetson 最大性能模式。
- 执行 `jetson_clocks`。
- 停止 `gdm3` 桌面服务。

检查通过后启动：

```bash
vision_up
```

它会在名为 `vision_up` 的 tmux 会话中执行：

```bash
source /home/meta/vision_ws/install/setup.bash
ros2 launch auto_aim_bringup debug.py camera_type:=mindvision profile:=infantry
```

重新进入 tmux：

```bash
tmux attach -t vision_up
```

离开 tmux 但保持程序运行：

```text
Ctrl-b
d
```

停止备用启动：

```bash
tmux kill-session -t vision_up
```

恢复正式服务：

```bash
sudo systemctl start auto_aim.service
```

## 6. 赛场外调试

完整参数含义和调试顺序见：

- [Z_LION_AutoAim2025 调参手册](./Z_LION_AutoAim2025调参手册.pdf)

本节只说明如何让机器进入适合调参的运行状态。

### 6.1 安全准备

调试前建议先在当前车型配置中关火：

```yaml
/serial_send_data_node:
  ros__parameters:
    close_shoot: true
```

例如步兵修改：

```text
src/auto_aim_bringup/config/infantry.yaml
```

需要实际测试发射时，再由负责调试的队员明确改回：

```yaml
close_shoot: false
```

### 6.2 停止自动服务

```bash
sudo systemctl stop auto_aim.service
```

确认没有残留视觉进程：

```bash
ps -ef | grep -E "ros2|auto_aim|camera_node" | grep -v grep
```

### 6.3 更新代码

先检查本地是否有车型参数未提交：

```bash
cd /home/meta/vision_ws
git status
```

确认可以更新后：

```bash
git switch tensorrt
git pull --ff-only origin tensorrt
```

不要在有未保存车型参数时直接执行覆盖或强制重置。

### 6.4 编译

首次编译或接口发生变化：

```bash
cd /home/meta/vision_ws
source /opt/ros/humble/setup.bash
colcon build --symlink-install --packages-select auto_aim_interfaces
source install/setup.bash
colcon build --symlink-install
```

普通 C++ 代码修改：

```bash
cd /home/meta/vision_ws
source /opt/ros/humble/setup.bash
source install/setup.bash
colcon build --symlink-install
```

只修改参数文件时，为确保安装目录中的参数同步，可以执行：

```bash
colcon build --symlink-install --packages-select auto_aim_bringup
source install/setup.bash
```

### 6.5 启动调试

推荐使用已经写入 `.bashrc` 的方式：

```bash
vision_check
vision_up
```

也可以直接前台运行：

```bash
cd /home/meta/vision_ws
source /opt/ros/humble/setup.bash
source install/setup.bash
ros2 launch auto_aim_bringup debug.py \
  camera_type:=mindvision \
  profile:=infantry
```

其他车型替换 `camera_type` 和 `profile`。

### 6.6 ROS 运行检查

查看节点：

```bash
ros2 node list
```

正常完整链路应包含相机、串口读取、串口发送、检测、解算和调试节点。

查看话题：

```bash
ros2 topic list
```

查看图像频率时，先从 `ros2 topic list` 找到相机图像话题，再执行：

```bash
ros2 topic hz <图像话题名>
```

调试完成后：

```bash
tmux kill-session -t vision_up
sudo systemctl start auto_aim.service
```

## 7. 新车部署

### 7.1 标准环境

当前已验证环境：

- NVIDIA Jetson Orin Nano。
- Ubuntu 22.04。
- ROS 2 Humble。
- CUDA 12。
- TensorRT 10。
- OpenCV、Eigen、Ceres、fmt。
- 海康或迈德威视工业相机 SDK。

建议新车统一使用：

```text
用户名：meta
工作空间：/home/meta/vision_ws
```

当前服务和模型路径包含以上绝对路径。如果用户名或目录不同，必须同步修改：

```text
tools/boot_run.sh
tools/install_boot_run.sh
src/auto_aim_bringup/config/*.yaml
```

### 7.2 安装基础依赖

先安装 ROS 2 Humble，并确保存在：

```bash
test -f /opt/ros/humble/setup.bash
```

安装常用构建依赖：

```bash
sudo apt update
sudo apt install -y \
  git \
  tmux \
  python3-colcon-common-extensions \
  python3-rosdep \
  libeigen3-dev \
  libceres-dev \
  libfmt-dev \
  libopencv-dev \
  ros-humble-cv-bridge \
  ros-humble-camera-info-manager \
  ros-humble-image-transport \
  ros-humble-image-transport-plugins \
  ros-humble-camera-calibration
```

Jetson 应通过对应 JetPack 安装 CUDA 和 TensorRT。检查：

```bash
test -f /usr/include/aarch64-linux-gnu/NvInfer.h || \
  test -f /usr/include/NvInfer.h

ldconfig -p | grep -E "libnvinfer|libnvonnxparser|libcudart"
```

如果找不到这些库，先完成与当前 JetPack 匹配的 CUDA/TensorRT 安装，不要直接复制其他机器的系统库。

### 7.3 安装相机 SDK

根据新车相机安装对应厂商 SDK 和 USB 规则。

迈德威视检查：

```bash
ldconfig -p | grep libMVSDK
```

海康检查：

```bash
ldconfig -p | grep libMvCameraControl
```

连接相机后检查：

```bash
lsusb
```

仓库中包含编译所需的相机 SDK 头文件和部分架构库，但新系统仍应完成厂商驱动、运行库和 USB 权限部署。

### 7.4 克隆正确分支

```bash
cd /home/meta
git clone --branch tensorrt --single-branch \
  https://github.com/LunateElf/Z_LION_AutoAim2025.git \
  vision_ws

cd /home/meta/vision_ws
git status
git branch --show-current
```

最后一条应输出：

```text
tensorrt
```

### 7.5 安装 ROS 依赖并编译

```bash
cd /home/meta/vision_ws
source /opt/ros/humble/setup.bash

sudo rosdep init 2>/dev/null || true
rosdep update
rosdep install --from-paths src --ignore-src -r -y

colcon build --symlink-install --packages-select auto_aim_interfaces
source install/setup.bash
colcon build --symlink-install
```

检查关键文件：

```bash
test -f install/setup.bash
test -f src/auto_aim/models/yolo.onnx
test -f src/auto_aim/models/mlp.onnx
```

### 7.6 选择车型和相机

先选择最接近的新车配置：

```text
英雄：hero
哨兵：sentry
步兵：infantry
```

然后根据实际相机选择：

```text
海康：hik
迈德威视：mindvision
```

修改 `tools/boot_run.sh` 最后一行。例如迈德威视步兵：

```bash
ros2 launch auto_aim_bringup debug.py camera_type:=mindvision profile:=infantry
```

海康英雄：

```bash
ros2 launch auto_aim_bringup debug.py camera_type:=hik profile:=hero
```

新车必须重新确认：

- 相机曝光、增益和旋转方向。
- 相机内参和畸变参数。
- 相机到云台及枪管的坐标偏移。
- yaw/pitch 读取和发送方向。
- 弹速、弹道补偿和发弹延迟。
- `close_shoot` 是否符合当前调试阶段。

具体调试顺序见 PDF 调参手册。

### 7.7 配置串口权限

```bash
sudo tee /etc/udev/rules.d/99-autoaim-serial.rules >/dev/null <<'EOF'
KERNEL=="ttyACM[0-9]*", MODE="0666"
KERNEL=="ttyUSB[0-9]*", MODE="0666"
EOF

sudo udevadm control --reload-rules
sudo udevadm trigger --subsystem-match=tty
```

重新插拔电控 USB 后检查：

```bash
ls -l /dev/ttyACM*
```

### 7.8 前台试运行

安装服务前先前台验证：

```bash
cd /home/meta/vision_ws
source /opt/ros/humble/setup.bash
source install/setup.bash
ros2 launch auto_aim_bringup debug.py \
  camera_type:=mindvision \
  profile:=infantry
```

确认相机、串口、TensorRT、识别和解算均正常后按 `Ctrl-C` 停止。

如果已有 TensorRT engine 与新 JetPack、TensorRT 版本或硬件不兼容，可以先备份旧 engine：

```bash
cd /home/meta/vision_ws/src/auto_aim/models
mv yolo.engine yolo.engine.incompatible.bak
```

下次启动时程序会尝试从 `yolo.onnx` 重新构建。

### 7.9 安装开机服务

确认 `tools/boot_run.sh` 中的相机和车型参数正确后：

```bash
cd /home/meta/vision_ws
chmod +x tools/boot_run.sh tools/install_boot_run.sh
sudo -v
./tools/install_boot_run.sh
```

安装脚本会：

- 写入 `/etc/systemd/system/auto_aim.service`。
- 停止并屏蔽旧 `rc-local.service`。
- 重新加载 systemd。
- 启用并立即启动 `auto_aim.service`。

检查：

```bash
systemctl is-enabled auto_aim.service
systemctl is-active auto_aim.service
systemctl is-enabled rc-local.service
```

预期：

```text
enabled
active
masked
```

查看日志：

```bash
journalctl -u auto_aim.service -f
```

### 7.10 配置 `.bashrc` 备用命令

以下示例对应迈德威视步兵。其他车型需要替换两处 `mindvision` 和 `infantry`。

将以下内容加入 `~/.bashrc`：

```bash
source /opt/ros/humble/setup.bash

alias vision_check="sudo bash /home/meta/vision_ws/scripts/run_auto_aim_match.sh -c mindvision -p infantry"

vision_up() {
    local cmd="source /home/meta/vision_ws/install/setup.bash && ros2 launch auto_aim_bringup debug.py camera_type:=mindvision profile:=infantry"

    if ! command -v tmux >/dev/null 2>&1; then
        echo "[vision_up] tmux not found" >&2
        return 1
    fi

    tmux has-session -t vision_up 2>/dev/null || tmux new-session -d -s vision_up
    tmux new-window -t vision_up "bash -lc '$cmd'"

    if [ -n "${TMUX:-}" ]; then
        tmux switch-client -t vision_up
    else
        tmux attach -t vision_up
    fi
}
```

使配置立即生效：

```bash
source ~/.bashrc
type vision_check
type vision_up
```

### 7.11 上电验收

服务部署完成后必须进行一次真实重启测试：

```bash
sudo reboot
```

机器重新上线后检查：

```bash
systemctl status auto_aim.service --no-pager -l
journalctl -u auto_aim.service -b --no-pager
```

验收项目：

1. 不登录 SSH，视觉也能随上电启动。
2. 串口未连接时服务等待，不反复崩溃。
3. 串口接入后只启动一套视觉节点。
4. 相机正常出图。
5. TensorRT engine 正常加载或构建。
6. 串口收发正常。
7. 断开 SSH 不影响视觉运行。
8. 服务异常退出后能够由 systemd 自动恢复。

## 8. 已部署机器更新流程

不要直接在比赛前覆盖一台已经调好的机器。更新前先保存车型参数。

```bash
cd /home/meta/vision_ws
git status
git diff
```

推荐更新流程：

```bash
sudo systemctl stop auto_aim.service

cd /home/meta/vision_ws
git switch tensorrt
git pull --ff-only origin tensorrt

source /opt/ros/humble/setup.bash
source install/setup.bash
colcon build --symlink-install

sudo systemctl restart auto_aim.service
systemctl status auto_aim.service --no-pager -l
```

车型参数应单独提交，或者在更新前导出补丁：

```bash
git diff -- src/auto_aim_bringup/config \
  src/hik_camera/config \
  src/rm_vision_ros2_mindvision_camera/config \
  > ~/vehicle_params.patch
```

## 9. 常见问题

### 9.1 服务一直等待

```bash
ls -l /dev/ttyACM*
```

无输出说明电控串口没有枚举。检查 USB 线、电控供电和 udev。

### 9.2 相机打不开

```bash
lsusb
ldconfig -p | grep -E "libMVSDK|libMvCameraControl"
```

同时确认 `camera_type` 与实际相机一致。

### 9.3 TensorRT 启动失败

```bash
ldconfig -p | grep -E "libnvinfer|libnvonnxparser|libcudart"
```

检查 JetPack、CUDA、TensorRT 与 engine 是否兼容。必要时备份旧 engine，让程序从 ONNX 重建。

### 9.4 参数修改没有生效

```bash
cd /home/meta/vision_ws
colcon build --symlink-install --packages-select auto_aim_bringup
source install/setup.bash
sudo systemctl restart auto_aim.service
```

### 9.5 相机或串口被占用

```bash
systemctl is-active auto_aim.service
tmux list-sessions
ps -ef | grep -E "ros2|auto_aim|camera_node" | grep -v grep
```

确保 systemd 和手动 tmux 没有同时运行。

### 9.6 紧急停止视觉

```bash
sudo systemctl stop auto_aim.service
tmux kill-session -t vision_up 2>/dev/null || true
```

## 10. 交付要求

每台机器人部署完成后至少记录：

- 机器人名称和兵种。
- Orin 主机名。
- 相机类型和序列号。
- 使用的 Git 分支和提交 SHA。
- 使用的 `profile`。
- 相机标定日期。
- 坐标系偏移参数。
- 弹速、补偿和发弹延迟。
- systemd 上电测试结果。
- 最终调试视频和关键参数。

记录当前提交：

```bash
cd /home/meta/vision_ws
git branch --show-current
git rev-parse HEAD
git status
```

比赛前应确保：

```bash
systemctl is-enabled auto_aim.service
systemctl is-active auto_aim.service
systemctl is-enabled rc-local.service
```

对应状态为：

```text
enabled
active
masked
```
