#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include "util.h"

int logmask = 0;

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
log_printf(int mask, char *fmt, ...)
{
  va_list args;

  if (mask & logmask) {
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fflush(stderr);
  }
}

void
log_print_data(int mask, unsigned int size, unsigned char *buf)
{
  if (mask & logmask) {
    for (; size > 0; size--, buf++)
      fprintf(stderr, "%02x ", *buf);
    fprintf(stderr, "\n");
    fflush(stderr);
  }
}

int
arr_memcpy(Arr *a, int delta, int off, int size, const void *data)
{
  unsigned int u = 0, s = 0, curused = 0, cursize = 0;

  if (*a) {
    curused = u = (*a)->used;
    cursize = s = (*a)->size;
  }
  if (off < 0)
    off = u;
  if (off + size > s && delta == 0)
    return -1;
  for (; off + size >= s; s += delta) {}
  if (s > cursize) {
    *a = realloc(*a, sizeof(struct arr) - sizeof(int) + s);
    if (!*a)
      return -1;
    (*a)->size = s;
  }
  if (data)
    memcpy((*a)->b + off, data, size);
  if (off + size > u)
    (*a)->used = off + size;
  return curused;
}

int
arr_delete(Arr *a, unsigned int off, unsigned int size)
{
  unsigned int end;

  if (!(a && *a))
    return -1;

  if (off > (*a)->used)
    return -1;
  if (off + size > (*a)->used)
    size = (*a)->used - off;

  end = off + size;
  memmove((*a)->b + off, (*a)->b + end, (*a)->used - end);
  (*a)->used -= size;
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


