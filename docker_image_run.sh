#!/bin/bash

export IP=$(ifconfig en0 | grep inet | awk '$1=="inet" {print $2}')

xhost + $IP

docker build -t filemanagerdev /Users/david/Documents/Classes/CSCI490/E7FileManager

# Not working due /usr/local/bin/devcontainer script not launching not finding exectuable because of bad configuration
#devcontainer open /Users/david/Documents/Classes/CSCI490/E7FileManager/ 

docker run -itd --name filemanagerdevinst -e DISPLAY=$IP:0 \
    -v /tmp/.X11-unix/:/tmp/.X11-unix/ \
    -v /Users/david/.ssh/:/root/.ssh \
    -v /Users/david/Documents/Classes/CSCI490/E7FileManager/:/project/ \
    filemanagerdev /bin/bash
