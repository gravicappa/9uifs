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
log_print_data(int level, unsigned int size, unsigned char *buf)
{
  if (level <= loglevel) {
    for (; size > 0; size--, buf++)
      log_printf(level, "%02x ", *buf);
    log_printf(level, "\n");
  }
}

int
add_data(struct buf *buf, int size, const void *data)
{
  int s = buf->size, off = buf->used;

  while (buf->used + size >= s)
    s += buf->delta;
  if (s > buf->size) {
    buf->size = s;
    buf->b = (char *)realloc(buf->b, buf->size);
    if (!buf->b)
      return -1;
  }
  if (data)
    memcpy(buf->b + buf->used, data, size);
  buf->used += size;
  return off;
}

int
rm_data(struct buf *buf, int size, void *ptr)
{
  int off, end;

  if ((char *)ptr < buf->b || (char *)ptr + size > buf->b + buf->used)
    return -1;

  off = (char *)ptr - buf->b;
  end = off + size;
  memmove(buf->b + off, buf->b + end, buf->used - end);
  buf->used -= size;
  return 0;
}

char *
strnchr(const char *s, unsigned int len, char c)
{
  for (; len && *s && *s != c; --len, ++s) {}
  return (char *)((len > 0 && *s == c) ? s : 0);
}

char *
next_arg(char **s)
{
  char *b = *s;

  if (b) {
    *s = strchr(b, ' ');
    if (*s)
      *(*s)++ = 0;
  }
  return b;
}


