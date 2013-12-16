#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "util.h"
#include "9p.h"
#include "fs.h"
#include "client.h"

void
rm_fid_aux(struct p9_fid *fid)
{
  if (fid->aux)
    free(fid->aux);
  fid->aux = 0;
  fid->rm = 0;
}

void
read_data_fn(struct p9_connection *c, int size, char *buf)
{
  c->r.count = 0;
  if (c->t.offset < size) {
    c->r.count = c->t.count;
    if (c->t.offset + c->t.count > size)
      c->r.count = size - c->t.offset;
    c->r.data = buf + c->t.offset;
  }
}

void
read_str_fn(struct p9_connection *c, int size, char *buf)
{
  int i;
  char *s;
  if (buf) {
    for (s = buf, i = 0; i < size && *s; ++i, ++s) {}
    read_data_fn(c, i, buf);
  }
}

void
write_data_fn(struct p9_connection *c, int size, char *buf)
{
  c->r.count = c->t.count;
  if (c->t.offset >= size)
    return;
  if (c->t.offset + c->t.count > size)
    c->r.count = size - c->t.offset;
  memcpy(buf + c->t.offset, c->t.data, c->r.count);
}

void
write_buf_fn(struct p9_connection *c, int delta, struct arr **buf)
{
  int off, u;

  u = (*buf) ? (*buf)->used : 0;
  off = arr_memcpy(buf, delta, c->t.offset, c->t.count + 1, 0);
  if (off < 0) {
    P9_SET_STR(c->r.ename, "Cannot allocate memory");
    return;
  }
  memset((*buf)->b + u, 0, (*buf)->used - u);
  write_data_fn(c, (*buf)->size - 1, (*buf)->b);
}

void
read_bool_fn(struct p9_connection *c, int val)
{
  c->r.count = 0;
  if (c->t.offset == 0) {
    c->r.count = 1;
    c->r.data = val ? "1" : "0";
  }
}

int
write_bool_fn(struct p9_connection *c, int oldval)
{
  switch (c->t.count) {
  case 0: return oldval;
  case 1:
    c->r.count = 1;
    return c->t.data[0] != '0';
  default:
    c->r.count = c->t.count;
    return !(c->t.data[0] == '0' && strchr("\n\t\r ", c->t.data[1]));
  }
}

struct file *
find_file(char *name, struct file *root)
{
  struct file *t = root;
  char *p, *q;
  int size;
  size = strlen(name);

  p = name;
  while (t && p[0] && size > 0) {
    q = strchr(p, '/');
    if (q) {
      size -= q - p + 1;
      if (q > p)
        t = get_file(t, q - p, p);
      p = q + 1;
    } else
      return get_file(t, size, p);
  }
  return 0;
}

struct file *
find_file_global(char *name, struct client *c, int *global)
{
  unsigned long long id;
  for (; *name == '/'; ++name) {}
  *global = (sscanf(name, "%llu/", &id) == 1);
  if (*global) {
    c = client_by_id(id);
    for (; *name && *name != '/'; ++name) {}
    for (; *name == '/'; ++name) {}
  }
  return (c) ? find_file(name, &c->f) : 0;
}

void
strcpyrev(char *to, char *from, int n)
{
  if (n) {
    from += n - 1;
    while (n--)
      *to++ = *from--;
  }
}

int
file_path_len(struct file *f, struct file *root)
{
  int n = 0;
  for (; f && f != root; f = f->parent)
    n += strlen(f->name) + 1;
  return n;
}

int
file_path(int bytes, char *buf, struct file *f, struct file *root)
{
  int i, n, t, off = 0;

  for (; f && f != root; f = f->parent) {
    n = strlen(f->name);
    if (off + n >= bytes)
      return bytes - off - n;
    strcpyrev(buf + off, f->name, n);
    off += n;
    buf[off++] = '/';
  }
  buf[off - 1] = 0;
  n = off - 1;
  i = (n >> 1) - 1;
  for (; i >= 0; --i) {
    t = buf[i];
    buf[i] = buf[n - i - 1];
    buf[n - i - 1] = t;
  }
  return off;
}
