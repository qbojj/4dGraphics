FROM archlinux

# add chaotic-aur repo
RUN --mount=type=cache,target=/var/cache/pacman/pkg \
  pacman -Sy --noconfirm && \
  pacman-key --init && \
  pacman-key --recv-key 3056513887B78AEB --keyserver keyserver.ubuntu.com && \
  pacman-key --lsign-key 3056513887B78AEB && \
  pacman -U 'https://cdn-mirror.chaotic.cx/chaotic-aur/chaotic-keyring.pkg.tar.zst' --noconfirm && \
  pacman -U 'https://cdn-mirror.chaotic.cx/chaotic-aur/chaotic-mirrorlist.pkg.tar.zst' --noconfirm && \
  echo -e '[multilib]\nInclude = /etc/pacman.d/mirrorlist' >> /etc/pacman.conf && \
  echo -e '[chaotic-aur]\nInclude = /etc/pacman.d/chaotic-mirrorlist' >> /etc/pacman.conf

# install the toolchain and vcpkg dependencies
RUN --mount=type=cache,target=/var/cache/pacman/pkg \
    pacman -Syu --noconfirm \
    base-devel gcc llvm-git clang-git \
    git git-lfs cmake ninja ccache \
    curl zip unzip tar

# install vcpkg package dependencies
RUN --mount=type=cache,target=/var/cache/pacman/pkg \
    pacman -S --noconfirm \
    libxcb libx11 libxrandr python-jinja \
    libxft libxext libxkbcommon libibus

ENV CC=clang CXX=clang++