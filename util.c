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
arr_memcpy(struct arr **a, int delta, int off, int size, const void *data)
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
arr_add(struct arr **a, int delta, int size, const void *data)
{
  return arr_memcpy(a, delta, (*a) ? (*a)->used : 0, size, data);
}

int
arr_delete(struct arr **a, unsigned int off, unsigned int size)
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

int
arr_shrink(struct arr **a)
{
  if (!(a && *a))
    return -1;
  *a = realloc(*a, sizeof(struct arr) - sizeof(int) + (*a)->used);
  if (!*a)
    return -1;
  return ((*a)->size = (*a)->used);
}

char *
strnchr(const char *s, unsigned int len, char c)
{
  for (; len && *s && *s != c; --len, ++s) {}
  return (char *)((len > 0 && *s == c) ? s : 0);
}

#define ISWS(c) ((c) == ' ' || (c) == '\n' || (c) == '\r' || (c) == '\t')

int
nargs(char *s)
{
  int n = 0;
  if (!s)
    return 0;
  while (*s) {
    for (;*s && ISWS(*s); ++s) {}
    if (*s && !ISWS(*s)) {
      ++n;
      for (;*s && !ISWS(*s); ++s) {}
    }
  }
  return n;
}

char *
next_arg(char **s)
{
  char *b, *ret = *s;

  if (ret) {
    for (; *ret && ISWS(*ret); ++ret) {}
    if (!*ret) {
      *s = ret;
      return 0;
    }
    for (b = ret; *b && !ISWS(*b); ++b) {}
    *s = (*b) ? b + 1 : b;
    *b = 0;
  }
  return ret;
}

/* TODO: convert \" into " somehow */
char *
next_quoted_arg(char **s)
{
  char *b, *ret = *s;
  int back = 0;

  if (ret) {
    for (; *ret && ISWS(*ret); ++ret) {}
    if (*ret != '"')
      return next_arg(s);
    ++ret;
    for (b = ret; *b && (*b != '"' || back); ++b)
      back = (*b == '\\' && !back);
    *s = (*b) ? b + 1 : b;
    *b = 0;
  }
  return ret;
}

char *
trim_string_right(char *s, char *chars)
{
  int len = strlen(s), i;
  for (i = len; i >= 0 && strchr(chars, s[i]); --i) {}
  if (i >= 0 && i < len)
    s[i + 1] = 0;
  return s;
}

char *
utf8_from_rune(unsigned long rune, char buf[8])
{
  int i = 0, s = -6;
  if (rune <= 0x7f)
    buf[i++] = rune;
  else if (rune <= 0x7ff) {
    s = 0;
    buf[i++] = 0xc0 | (rune >> 6);
  } else if (rune <= 0xffff) {
    s = 6;
    buf[i++] = 0xe0 | (rune >> 12);
  } else if (rune <= 0x1fffff) {
    s = 12;
    buf[i++] = 0xf0 | (rune >> 18);
  } else if (rune <= 0x3ffffff) {
    s = 18;
    buf[i++] = 0xf8 | (rune >> 24);
  } else if (rune <= 0x7fffffff) {
    s = 24;
    buf[i++] = 0xfc | (rune >> 30);
  }
  for (; s >= 0; s -= 6)
    buf[i++] = 0x80 | ((rune >> s) & 0x3f);
  buf[i] = 0;
  return buf;
}
