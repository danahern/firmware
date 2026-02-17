FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# Buildroot build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    bison \
    flex \
    libssl-dev \
    libncurses-dev \
    unzip \
    wget \
    curl \
    git \
    bc \
    cpio \
    rsync \
    python3 \
    file \
    locales \
    && rm -rf /var/lib/apt/lists/*

# UTF-8 locale (required by Buildroot menuconfig)
RUN locale-gen en_US.UTF-8
ENV LANG=en_US.UTF-8
ENV LC_ALL=en_US.UTF-8

# Buildroot refuses to build as root
RUN useradd -m -s /bin/bash builder \
    && mkdir -p /artifacts \
    && chown builder:builder /artifacts
USER builder
WORKDIR /home/builder

CMD ["sleep", "infinity"]
