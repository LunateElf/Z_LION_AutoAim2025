#!/usr/bin/env bash
set -euo pipefail

WORKSPACE="/home/meta/vision_ws"
CAMERA_TYPE="hik"
PROFILE="hero"
STOP_DESKTOP=1
MAX_PERF=1

UDEV_RULE_FILE="/etc/udev/rules.d/99-autoaim-serial.rules"
UDEV_RULE_CONTENT=$'KERNEL=="ttyACM[0-9]*", MODE="0666"\nKERNEL=="ttyUSB[0-9]*", MODE="0666"\n'

info() { echo "[INFO] $*"; }
warn() { echo "[WARN] $*" >&2; }
err()  { echo "[ERROR] $*" >&2; }

usage() {
  cat <<EOF
Usage:
  $(basename "$0") [options]

Options:
  -c, --camera-type <hik|mindvision>   Camera driver to launch (default: hik)
  -p, --profile <default|hero|sentry|infantry>
                                        Auto-aim profile name (default: hero)
  -h, --help                            Show this help

Examples:
  $(basename "$0") -c hik -p hero
  $(basename "$0") -c mindvision -p infantry
EOF
}

need_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    err "Command not found: $1"
    exit 1
  fi
}

require_sudo() {
  if sudo -n true 2>/dev/null; then
    return
  fi
  info "Sudo authentication is required for performance/desktop/serial-permission operations"
  sudo -v
}

parse_args() {
  while [[ $# -gt 0 ]]; do
    case "$1" in
      -c|--camera-type)
        CAMERA_TYPE="${2:-}"
        shift 2
        ;;
      -p|--profile)
        PROFILE="${2:-}"
        shift 2
        ;;
      -h|--help)
        usage
        exit 0
        ;;
      *)
        err "Unknown option: $1"
        usage
        exit 1
        ;;
    esac
  done
}

validate_args() {
  case "$CAMERA_TYPE" in
    hik|mindvision) ;;
    *)
      err "Invalid camera type: $CAMERA_TYPE (must be hik or mindvision)"
      exit 1
      ;;
  esac

  case "$PROFILE" in
    default|hero|sentry|infantry) ;;
    *)
      err "Invalid profile: $PROFILE (must be default/hero/sentry/infantry)"
      exit 1
      ;;
  esac
}

check_workspace() {
  if [[ ! -d "$WORKSPACE" ]]; then
    err "Workspace not found: $WORKSPACE"
    exit 1
  fi

  if [[ ! -f "$WORKSPACE/install/setup.bash" ]]; then
    err "Missing setup file: $WORKSPACE/install/setup.bash"
    err "Please build first: cd $WORKSPACE && colcon build"
    exit 1
  fi

  local profile_yaml="$WORKSPACE/src/auto_aim_bringup/config/$PROFILE.yaml"
  if [[ ! -f "$profile_yaml" ]]; then
    err "Profile config not found: $profile_yaml"
    exit 1
  fi

  local cam_info=""
  if [[ "$CAMERA_TYPE" == "hik" ]]; then
    cam_info="$WORKSPACE/src/hik_camera/config/camera_info_${PROFILE}.yaml"
  else
    cam_info="$WORKSPACE/src/rm_vision_ros2_mindvision_camera/config/camera_info_${PROFILE}.yaml"
  fi
  if [[ ! -f "$cam_info" ]]; then
    warn "Camera info file not found for selected profile: $cam_info"
    warn "Launch may still run if default camera_info.yaml is used, but calibration can be wrong"
  fi

  local yolo_model="$WORKSPACE/src/auto_aim/models/yolo.onnx"
  local number_model="$WORKSPACE/src/auto_aim/models/mlp.onnx"
  [[ -f "$yolo_model" ]] || { err "Missing model: $yolo_model"; exit 1; }
  [[ -f "$number_model" ]] || { err "Missing model: $number_model"; exit 1; }
}

ensure_serial_permission_persistent() {
  local tmp_rule
  tmp_rule="$(mktemp)"
  printf "%s" "$UDEV_RULE_CONTENT" > "$tmp_rule"

  if [[ ! -f "$UDEV_RULE_FILE" ]] || ! cmp -s "$tmp_rule" "$UDEV_RULE_FILE"; then
    info "Installing persistent serial permission rule: $UDEV_RULE_FILE"
    sudo cp "$tmp_rule" "$UDEV_RULE_FILE"
    sudo chmod 644 "$UDEV_RULE_FILE"
    sudo udevadm control --reload-rules
    sudo udevadm trigger --subsystem-match=tty || true
  fi

  rm -f "$tmp_rule"
}

check_serial() {
  local found=0
  local rw_ok=0

  info "Checking serial devices /dev/ttyACM0..9"
  for i in {0..9}; do
    local dev="/dev/ttyACM$i"
    if [[ -e "$dev" ]]; then
      found=1
      if [[ -r "$dev" && -w "$dev" ]]; then
        rw_ok=1
        info "Serial ready: $dev"
      else
        warn "Serial exists but permission is insufficient: $dev"
      fi
    fi
  done

  if [[ $found -eq 0 ]]; then
    err "No /dev/ttyACM* device found; launch aborted"
    exit 1
  fi

  if [[ $rw_ok -eq 0 ]]; then
    err "Found /dev/ttyACM* but none has read/write permission; launch aborted"
    exit 1
  fi
}

check_camera_hint() {
  if ! command -v lsusb >/dev/null 2>&1; then
    return
  fi

  if [[ "$CAMERA_TYPE" == "hik" ]]; then
    if ! lsusb | grep -Eiq "hik|hikrobot|mv"; then
      warn "No obvious Hik camera entry in lsusb; verify USB cable and camera power"
    fi
  else
    if ! lsusb | grep -Eiq "mindvision|mv"; then
      warn "No obvious MindVision camera entry in lsusb; verify USB cable and camera power"
    fi
  fi
}

apply_max_perf() {
  info "Applying Jetson max performance settings"
  if command -v nvpmodel >/dev/null 2>&1; then
    if ! sudo nvpmodel -m 0; then
      warn "nvpmodel -m 0 failed; keeping current power mode"
    fi
    sudo nvpmodel -q || true
  else
    warn "nvpmodel not found; skip power mode setup"
  fi

  if command -v jetson_clocks >/dev/null 2>&1; then
    if ! sudo jetson_clocks; then
      warn "jetson_clocks failed"
    fi
  else
    warn "jetson_clocks not found; skip clocks lock"
  fi
}

stop_desktop_if_needed() {
  if [[ $STOP_DESKTOP -eq 0 ]]; then
    return
  fi

  if ! systemctl list-unit-files | grep -q '^gdm3.service'; then
    warn "gdm3.service not found; skip desktop stop"
    return
  fi

  if systemctl is-active --quiet gdm3; then
    info "Stopping gdm3 (headless mode)"
    sudo systemctl stop gdm3
  else
    info "gdm3 is already inactive"
  fi
}

source_env() {
  local had_nounset=0
  if [[ $- == *u* ]]; then
    had_nounset=1
    set +u
  fi

  if [[ -f /opt/ros/humble/setup.bash ]]; then
    # shellcheck disable=SC1091
    source /opt/ros/humble/setup.bash
  fi

  # shellcheck disable=SC1090
  source "$WORKSPACE/install/setup.bash"

  if [[ $had_nounset -eq 1 ]]; then
    set -u
  fi

  need_cmd ros2
}

setup_display_for_ssh_debug() {
  if [[ -n "${SSH_CLIENT:-}" ]]; then
    export DISPLAY="$(echo "$SSH_CLIENT" | awk '{print $1}'):0.0"
    info "DISPLAY set to $DISPLAY (from SSH_CLIENT)"
    return
  fi

  # Fallback for cases like sudo env reset where SSH_CLIENT may be stripped.
  local ssh_ip
  ssh_ip="$(who -m 2>/dev/null | awk '{gsub(/[()]/, "", $NF); print $NF}')"
  if [[ -n "$ssh_ip" ]]; then
    export DISPLAY="$ssh_ip:0.0"
    info "DISPLAY set to $DISPLAY (from who -m fallback)"
  else
    warn "SSH client IP not found; keep DISPLAY=${DISPLAY:-<unset>}"
  fi
}

main() {
  parse_args "$@"
  validate_args

  info "Workspace    : $WORKSPACE"
  info "Camera type  : $CAMERA_TYPE"
  info "Profile      : $PROFILE"
  info "Stop desktop : $STOP_DESKTOP"
  info "Max perf     : $MAX_PERF"

  check_workspace

  need_cmd sudo
  require_sudo
  ensure_serial_permission_persistent

  check_serial
  check_camera_hint

  if [[ $MAX_PERF -eq 1 ]]; then
    apply_max_perf
  fi

  stop_desktop_if_needed

  cd "$WORKSPACE"
  source_env
  setup_display_for_ssh_debug

  info "Launching auto aim"
  info "Command: ros2 launch auto_aim_bringup debug.py camera_type:=$CAMERA_TYPE profile:=$PROFILE"
  ros2 launch auto_aim_bringup debug.py \
    camera_type:="$CAMERA_TYPE" \
    profile:="$PROFILE"
}

main "$@"
