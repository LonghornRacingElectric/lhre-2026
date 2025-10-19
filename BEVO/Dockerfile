# /BEVO/Dockerfile

FROM arm64v8/ubuntu:22.04

# Set non-interactive mode for package installations
ENV DEBIAN_FRONTEND=noninteractive

# Rebuild the sources.list from scratch for multi-arch support
# This is the most robust way to avoid 404 errors
RUN apt-get update && apt-get install -y curl wget && \
    dpkg --add-architecture amd64 && \
    # Overwrite the sources.list file completely
    echo "deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports jammy main restricted universe multiverse" > /etc/apt/sources.list && \
    echo "deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports jammy-updates main restricted universe multiverse" >> /etc/apt/sources.list && \
    echo "deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports jammy-backports main restricted universe multiverse" >> /etc/apt/sources.list && \
    echo "deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports jammy-security main restricted universe multiverse" >> /etc/apt/sources.list && \
    # Add the amd64 sources
    echo "deb [arch=amd64] http://archive.ubuntu.com/ubuntu jammy main restricted universe multiverse" >> /etc/apt/sources.list && \
    echo "deb [arch=amd64] http://archive.ubuntu.com/ubuntu jammy-updates main restricted universe multiverse" >> /etc/apt/sources.list && \
    echo "deb [arch=amd64] http://archive.ubuntu.com/ubuntu jammy-backports main restricted universe multiverse" >> /etc/apt/sources.list && \
    echo "deb [arch=amd64] http://security.ubuntu.com/ubuntu jammy-security main restricted universe multiverse" >> /etc/apt/sources.list && \
    # Now, update with the new, correct sources
    apt-get update && \
    # Install all packages
    apt-get install -y \
    libc6:amd64 \
    build-essential \
    git \
    curl \
    wget \
    unzip \
    qemu-user-static \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*
# Install Rust using rustup
RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
ENV PATH="/root/.cargo/bin:${PATH}"

# Install the native ARM version of Bazelisk
RUN wget https://github.com/bazelbuild/bazelisk/releases/download/v1.19.0/bazelisk-linux-arm64 -O /usr/local/bin/bazel && \
    chmod +x /usr/local/bin/bazel

# Set the working directory to the monorepo root
WORKDIR /workspaces
