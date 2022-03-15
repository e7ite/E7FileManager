FROM debian:bullseye

ENV DEBIAN_FRONTEND=noninteractive 

RUN apt-get update 

RUN apt-get install libgtkmm-3.0 man build-essential cmake git procps wget \
    clang-tidy-11 clang-format-11 valgrind -yq
