FROM ubuntu:22.04

ARG DEBIAN_FRONTEND=noninteractive
ARG UBUNTU_MIRROR=mirrors.tuna.tsinghua.edu.cn
ARG QT_VERSION=6.8.2
ARG QT_HOST=linux
ARG QT_TARGET=desktop
ARG QT_ARCH=linux_gcc_64
ARG LINUXDEPLOYQT_URL=https://github.com/probonopd/linuxdeployqt/releases/download/continuous/linuxdeployqt-continuous-x86_64.AppImage

ENV QT_ROOT=/opt/Qt/${QT_VERSION}/gcc_64
ENV PATH=/opt/Qt/${QT_VERSION}/gcc_64/bin:${PATH}
ENV CMAKE_PREFIX_PATH=/opt/Qt/${QT_VERSION}/gcc_64
ENV LD_LIBRARY_PATH=/opt/Qt/${QT_VERSION}/gcc_64/lib

RUN sed -i "s|http://archive.ubuntu.com/ubuntu/|http://${UBUNTU_MIRROR}/ubuntu/|g; s|http://security.ubuntu.com/ubuntu/|http://${UBUNTU_MIRROR}/ubuntu/|g" /etc/apt/sources.list \
    && apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        cmake \
        curl \
        file \
        fuse \
        g++ \
        git \
        libfontconfig1 \
        libfuse2 \
        libgl1 \
        libgl1-mesa-dev \
        libmpv-dev \
        libpulse0 \
        libssl3 \
        libvulkan1 \
        libxcb-cursor0 \
        libxcb-icccm4 \
        libxcb-image0 \
        libxcb-keysyms1 \
        libxcb-render-util0 \
        libxcb-shape0 \
        libxcb-xinerama0 \
        libxcb-xkb1 \
        libxkbcommon-x11-0 \
        make \
        ninja-build \
        patchelf \
        pkg-config \
        python3 \
        python3-pip \
        zsync \
    && rm -rf /var/lib/apt/lists/*

RUN python3 -m pip install --no-cache-dir aqtinstall \
    && python3 -m aqt install-qt "${QT_HOST}" "${QT_TARGET}" "${QT_VERSION}" "${QT_ARCH}" \
        -O /opt/Qt \
        -m qtshadertools \
    && curl -fsSL "${LINUXDEPLOYQT_URL}" -o /usr/local/bin/linuxdeployqt \
    && chmod +x /usr/local/bin/linuxdeployqt

ENV APPIMAGE_EXTRACT_AND_RUN=1

WORKDIR /src
COPY . /src

ENTRYPOINT ["/src/scripts/package-appimage.sh"]
