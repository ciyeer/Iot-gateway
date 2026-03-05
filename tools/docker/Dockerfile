# 使用 Ubuntu 20.04 作为基础镜像
FROM ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive

# 增加构建参数，控制是否使用国内源 (默认使用，方便国内开发者)
ARG USE_CHINA_MIRROR=true

# ARM 镜像必须替换 ports.ubuntu.com
# 如果 USE_CHINA_MIRROR 为 true，则使用阿里云源
RUN if [ "$USE_CHINA_MIRROR" = "true" ]; then \
        sed -i 's|http://ports.ubuntu.com|http://mirrors.aliyun.com|g' /etc/apt/sources.list; \
    fi

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
