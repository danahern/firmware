FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=America/New_York
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

# Yocto build dependencies (scarthgap)
RUN apt-get update && apt-get install -y \
    bc build-essential chrpath cpio diffstat gawk git git-lfs \
    libssl-dev texinfo wget gdisk python3 python3-pip python3-pexpect \
    python3-git python3-jinja2 python3-subunit python3-distutils \
    xz-utils debianutils iputils-ping locales \
    socat unzip file zstd liblz4-tool \
    libgmp-dev libmpc-dev lz4 \
    sudo vim \
    && rm -rf /var/lib/apt/lists/*

# Set locale (Yocto requires UTF-8)
RUN locale-gen en_US.UTF-8
ENV LANG=en_US.UTF-8
ENV LC_ALL=en_US.UTF-8

# Yocto refuses to build as root — create build user
RUN groupadd -g 1000 builder && \
    useradd -u 1000 -g builder -d /home/builder -m -s /bin/bash builder && \
    echo 'builder:builder' | chpasswd && \
    usermod -aG sudo builder && \
    echo 'builder ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers

# Use bash (not dash) — Yocto requirement
RUN echo "dash dash/sh boolean false" | debconf-set-selections && \
    dpkg-reconfigure dash

USER builder
WORKDIR /home/builder
