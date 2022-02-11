FROM ubuntu:latest

ENV DEBIAN_FRONTEND=noninteractive 

RUN apt-get update 

RUN apt-get install libgtkmm-3.0 build-essential firefox cmake git procps wget \
    clang-tidy-11 clang-format-11 -yq
