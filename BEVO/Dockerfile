# Use an ARM64 base image as the sources are configured for it.
# Using a specific version tag (e.g., :22.04) is a good practice.
FROM arm64v8/ubuntu:jammy

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get install -y --no-install-recommends curl wget ca-certificates && \
    dpkg --add-architecture amd64

RUN echo "deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports jammy main restricted universe multiverse" > /etc/apt/sources.list && \
    echo "deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports jammy-updates main restricted universe multiverse" >> /etc/apt/sources.list && \
    echo "deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports jammy-backports main restricted universe multiverse" >> /etc/apt/sources.list && \
    echo "deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports jammy-security main restricted universe multiverse" >> /etc/apt/sources.list && \
    echo "deb [arch=amd64] http://archive.ubuntu.com/ubuntu jammy main restricted universe multiverse" >> /etc/apt/sources.list && \
    echo "deb [arch=amd64] http://archive.ubuntu.com/ubuntu jammy-updates main restricted universe multiverse" >> /etc/apt/sources.list && \
    echo "deb [arch=amd64] http://archive.ubuntu.com/ubuntu jammy-backports main restricted universe multiverse" >> /etc/apt/sources.list && \
    echo "deb [arch=amd64] http://security.ubuntu.com/ubuntu jammy-security main restricted universe multiverse" >> /etc/apt/sources.list

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    libc6:amd64 \
    build-essential \
    git \
    unzip \
    qemu-user-static \
    && rm -rf /var/lib/apt/lists/*
