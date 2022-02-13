#Requires -Version 3
# Gets your local IPv4 address, builds the Docker image for the project, and
# starts it in detached mode, so you can connect via a code editor such as VSCode.
#
# Expects VcXsrv to be installed
#
# Usage:
# .\docker_image_run.ps1
#
# Credit to Robin Kretzshcmar for this PowerShell script from
# https://dev.to/darksmile92/run-gui-app-in-linux-docker-container-on-windows-host-4kde
#
# Credit to FoxDeploy for how to get active internet adapter
# https://stackoverflow.com/questions/33283848/determining-internet-connection-using-powershell

$ActiveInternetAdapter = (Get-NetRoute | ? DestinationPrefix -eq '0.0.0.0/0' | Get-NetIPInterface | Where ConnectionState -eq 'Connected').InterfaceAlias

$IPv4Address = (Get-NetIPAddress -AddressFamily IPv4 -InterfaceAlias $ActiveInternetAdapter).IPAddress

set-variable -name DISPLAY -value "${IPv4Address}:0.0"

docker build -t filemanagerdev ${PSScriptRoot}
docker run -itd --rm -e DISPLAY=${DISPLAY} `
    -v "${PSScriptRoot}\:/project/" `
    -v "${HOME}\.ssh\:/root/.ssh/" `
    -w "/project/" `
    --name filemanagerdevinst `
    filemanagerdev /bin/bash