if [ ! -f config.mk ]; then
  cp docker_config.mk config.mk
fi

if ! cmp config.mk docker_config.mk >/dev/null 2>&1 ; then
  cp config.mk config.mk.$(date +%Y%m%d%H%M%S)
  cp docker_config.mk config.mk
fi

#docker run -u=$UID:$(id -g $USER) -it --security-opt seccomp=unconfined --rm --mount src=/"$(pwd)",target=/mnt/cilk-plus-futures,type=bind -w=/mnt/cilk-plus-futures  -e LD_LIBRARY_PATH=/mnt/cilk-plus-futures/SuperMalloc/release/lib cilkfut

docker run -u=$UID:$(id -g $USER) -it --security-opt seccomp=unconfined --rm -v$(pwd):/mnt/cilk-plus-futures -w=/mnt/cilk-plus-futures  -e LD_LIBRARY_PATH=/mnt/cilk-plus-futures/SuperMalloc/release/lib cilkfut
