# E7FileManager
Yet another GNU/Linux GTK File Manager

## About
A GTKMM application for GNU/Linux that intends to dig in to how file managers 
work. It is solely a reinvent-the-wheel type of project, as that is the way I 
learn best on how most projects work.

Read [this](https://docs.google.com/document/d/1HemMB4NQQw3XpyLmlN2RkYXBpGGoL-UKzxsKtxnNEeE/edit?usp=sharing) 
to obtain a high level overview of this project, the requirements, and how these
will be created internally.

## Features
- View all files on local file system
- Differentiate between files and folders
- Perform absolute directory changes
- Back, forward and up directory navigation buttons
- Change directories by clicking on folders
- Scrollable and resizeable window

## Preview
![Preview](/preview.png)

## Dependencies
- [Docker](https://www.docker.com/)
- [GTKMM](http://www.gtkmm.org/en/)
- [CMake](https://cmake.org)
- [Abseil](https://github.com/abseil/abseil-cpp)
- [GoogleTest](https://github.com/google/googletest)
- [Clang Tools](https://clang.llvm.org/docs/ClangTools.html)
- [X11 server](https://en.wikipedia.org/wiki/X_Window_System)

## Build Instructions
If developing on Windows, install [Xming](http://www.straightrunning.com/XmingNotes/).
If developing on Mac, install [XQuartz](www.xquartz.org).
If developing on a Debian-based GNU/Linux OS, install the `xorg` package.

If you are using Windows or Mac, you will need to setup X11 port forwarding with the
X11 applications listed above. This will not be covering how to do this.

This will show how to build this project using Docker. At the moment, Docker  is a requirement since there are some hardcoded file paths used in the program that are not of high priority to fix. 

1. Install [Docker](https://www.docker.com/) for your system and make sure it is
running.
2. `git clone --recursive https://github.com/e7ite/E7FileManager.git`
3. If on Windows, run `docker_image_run.ps1`. On Mac and GNU/Linux, run `docker_image_run.sh`.
4. Attach to the new Docker container with this or an analogous command ```docker exec -it filemanagerdevinst /bin/bash```.
5. `cd /project/`
6. `cmake -S . -B build`
7. `cd build`
8. `make` This might take some time since `clang-tidy` static analysis occurs here.
9. `ctest` 
10. To run the application, run `./e7fmgr`. Program will hang and not do anything 
if X11 port forwarding was not setup correctly. Otherwise the program should be displayed.
