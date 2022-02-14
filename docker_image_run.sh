#!/bin/bash
#
# MacOS script to build Docker image and run a Docker container with X11
# forwarding with XQuartz and git credentials. 
#
# Expects XQuartz to be installed
#
# Usage:
# ./docker_image_run.sh

greadlink_path="$(which greadlink)"
if [ -z "$greadlink_path" ]
then
    echo "greadlink needs to be installed. You can install via `brew install coreutils`"
    exit 1
fi

IP=$(ifconfig en0 | grep inet | awk '$1=="inet" {print $2}')

script_path=$(dirname "$(greadlink -f "$0")")

xhost + $IP

docker build -t filemanagerdev $script_path
docker run -itd --name filemanagerdevinst -e DISPLAY=$IP:0 \
    -v /tmp/.X11-unix/:/tmp/.X11-unix/ \
    -v "$HOME/.ssh/:/root/.ssh" \
    -v "$script_path/:/project/" \
    filemanagerdev /bin/bash
