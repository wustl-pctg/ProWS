FROM ubuntu:16.04
#RUN apk update && apk upgrade
#RUN apk add g++ make git cmake python2 binutils-dev
#RUN rm -rf /var/cache/apk
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
      cmake \
      binutils-dev \
      python \
    && update-alternatives --install "/usr/bin/ld" "ld" "/usr/bin/ld.gold" 20 \
    && update-alternatives --install "/usr/bin/ld" "ld" "/usr/bin/ld.bfd" 10 \
    && rm -rf /var/lib/apt/lists/*

#WORKDIR /app
#ADD . /app
