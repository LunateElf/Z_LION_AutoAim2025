#!/bin/bash
set -euo pipefail

service_file=/etc/systemd/system/auto_aim.service

cat <<'EOF' | sudo tee "$service_file" >/dev/null
[Unit]
Description=ROS 2 Auto Aim Bringup Service
After=network.target

[Service]
Type=simple
User=meta
ExecStart=/bin/bash /home/meta/vision_ws/tools/boot_run.sh
Restart=on-failure
RestartSec=3
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
EOF

# Keep only the dedicated systemd service as the boot path.
sudo systemctl disable --now rc-local.service >/dev/null 2>&1 || true
sudo systemctl mask rc-local.service >/dev/null 2>&1 || true
sudo systemctl daemon-reload
sudo systemctl enable --now auto_aim.service

echo "Installed and started auto_aim.service"
