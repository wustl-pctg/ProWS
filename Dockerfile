FROM ubuntu:16.04
RUN apt-get update \
    && apt-get upgrade -y \
    && apt-get install -y \
      g++ \
      gcc \
      make \
      automake \
      libtool \
      libnuma-dev \
      libc-dev \
      groff \
      git \
      wget \
      binutils-dev \
      python \
      libgsl-dev \
      libjpeg-dev \
    && update-alternatives --install "/usr/bin/ld" "ld" "/usr/bin/ld.gold" 20 \
    && update-alternatives --install "/usr/bin/ld" "ld" "/usr/bin/ld.bfd" 10 \
    && rm -rf /var/lib/apt/lists/*
