#!/bin/bash
vision_ws_dir=/home/meta/vision_ws
username=meta
cat << EOF | sudo tee /etc/rc.local
#!/bin/sh -e
nohup sudo -u $username $vision_ws_dir/tools/boot_run.sh > /home/$username/boot_run.log 2>&1 &
exit 0
EOF
sudo chmod +x /etc/rc.local
echo "Installed boot_run.sh to /etc/rc.local. It will run on next boot."
