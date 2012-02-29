#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include "util.h"

int loglevel = 0;

void
die(char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  fputc('\n', stderr);
  exit(1);
}

void
log_printf(int level, char *fmt, ...)
{
  va_list args;

  if (level <= loglevel) {
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fflush(stderr);
  }
}

void
log_print_data(int level, unsigned int bytes, unsigned char *buf)
{
  if (level <= loglevel) {
    for (; bytes > 0; bytes--, buf++)
      log_printf(level, "%02x ", *buf);
    log_printf(level, "\n");
  }
}

int
add_data(struct buf *buf, int bytes, const void *data)
{
  int s = buf->size, off = buf->used;

  while (buf->used + bytes >= s)
    s += buf->delta;
  if (s > buf->size) {
    buf->size = s;
    buf->b = (char *)realloc(buf->b, buf->size);
    if (!buf->b)
      return -1;
  }
  if (data)
    memcpy(buf->b + buf->used, data, bytes);
  buf->used += bytes;
  return off;
}
