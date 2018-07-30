#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#include "util.h"

ssize_t xread(int sd, void *buf, size_t len) {
    char *p = (char *)buf;
    ssize_t nrecv = 0;
    ssize_t rv;

    while (nrecv < len) {
        rv = read(sd, p, len - nrecv);
        if (0 > rv && errno == EINTR)
            continue;
        if (0 > rv)
            return -1;
        if (0 == rv)
            return 0;
        nrecv += rv;
        p += rv;
    }
    return nrecv;
}

ssize_t xwrite(int sd, const void *buf, size_t len) {
    char *p = (char *)buf;
    ssize_t nsent = 0;
    ssize_t rv;

    while (nsent < len) {
        rv = write(sd, p, len - nsent);
        if (0 > rv && (errno == EINTR || errno == EAGAIN))
            continue;
        if (0 > rv)
            return -1;
        nsent += rv;
        p += rv;
    }
    return nsent;
}

ssize_t read_header(int fd, byte *compress_type) {
    int checkbit;

    assert(compress_type != NULL);

    if (xread(fd, &checkbit, sizeof(int)) < 0){
        return -1;
    }
    if (checkbit != CHECKBIT) {
        printf("format error!\n");
        return -1;
    }

    if (xread(fd, compress_type, sizeof(byte)) < 0){
        return -1;
    }

    return 0;
}

ssize_t write_header(int fd, byte compress_type) {
    int checkbit = CHECKBIT;
    if (xwrite(fd, &checkbit, sizeof(int)) == (size_t)-1){
        return -1;
    }

    if (xwrite(fd, &compress_type, sizeof(byte)) == (size_t)-1) {
        return -1;
    }
    return 0;
}

