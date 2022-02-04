FROM debian:bullseye

RUN apt-get update && \
    apt-get install build-essential cmake git libgtkmm-3.0-dev x11-apps procps -yq
