# 使用 Ubuntu 20.04 作为基础镜像
FROM ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive

# ARM 镜像必须替换 ports.ubuntu.com
RUN sed -i 's|http://ports.ubuntu.com|http://mirrors.aliyun.com|g' /etc/apt/sources.list

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    git \
    g++-aarch64-linux-gnu \
    gcc-aarch64-linux-gnu \
    file \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace

CMD ["/bin/bash"]
