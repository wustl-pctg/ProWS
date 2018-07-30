#ifndef _UTIL_H_
#define _UTIL_H_

#include "../dedupdef.h"

/* File I/O with error checking */
ssize_t xread(int sd, void *buf, size_t len);
ssize_t xwrite(int sd, const void *buf, size_t len);

/* Process file header */
ssize_t read_header(int fd, byte *compress_type);
ssize_t write_header(int fd, byte compress_type);

#endif //_UTIL_H_

