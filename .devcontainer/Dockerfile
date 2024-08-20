FROM archlinux:latest

# add chaotic-aur repository
RUN --mount=type=cache,target=/var/cache/pacman/pkg \
    pacman -Syu --noconfirm && \
    pacman-key --init && \
    pacman-key --recv-key 3056513887B78AEB --keyserver keyserver.ubuntu.com && \
    pacman-key --lsign-key 3056513887B78AEB && \
    pacman --noconfirm -U 'https://cdn-mirror.chaotic.cx/chaotic-aur/chaotic-keyring.pkg.tar.zst' && \
    pacman --noconfirm -U 'https://cdn-mirror.chaotic.cx/chaotic-aur/chaotic-mirrorlist.pkg.tar.zst' && \
    echo -e "[chaotic-aur]\nInclude = /etc/pacman.d/chaotic-mirrorlist" >> /etc/pacman.conf && \
    pacman -Syu --noconfirm

RUN --mount=type=cache,target=/var/cache/pacman/pkg \
    pacman -S --noconfirm clang-git

# install git-lfs and vcpkg package dependencies
RUN --mount=type=cache,target=/var/cache/pacman/pkg \
    pacman -S --noconfirm \
    base-devel ccache git git-lfs gcc cmake ninja \
    curl zip unzip tar \
    libxcb libx11 libxrandr python-jinja \
    libxft libxext libxkbcommon libibus

ENV CC=clang CXX=clang++
