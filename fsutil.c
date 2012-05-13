#include <string.h>
#include "util.h"
#include "9p.h"
#include "fs.h"

void
read_buf_fn(struct p9_connection *c, int size, char *buf)
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
write_buf_fn(struct p9_connection *c, int size, char *buf)
{
  if (c->t.offset >= size) {
    P9_SET_STR(c->r.ename, "Illegal seek");
    return;
  }
  c->r.count = c->t.count;
  if (c->t.offset + c->t.count > size)
    c->r.count = size - c->t.count;
  memcpy(buf + c->t.offset, c->t.data, c->r.count);
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
find_file_path(struct file *root, int size, char *name)
{
  struct file *t = root;
  char *p, *q;

  p = name;
  while (t && p[0] && size > 0) {
    q = strchr(p, '/');
    if (q) {
      size -= q - p + 1;
      if (q > p)
        t = find_file(t, q - p, p);
      p = q + 1;
    } else
      return find_file(t, size, p);
  }
  return 0;
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
file_path(struct arr **buf, struct file *f, struct file *root)
{
  int i, n, t, off = 0;
  struct arr *b;

  for (; f && f != root; f = f->parent) {
    n = strlen(f->name);
    if (arr_memcpy(buf, 16, off, n + 1, 0) < 0)
      return -1;
    strcpyrev((*buf)->b + off, f->name, n);
    off += n;
    (*buf)->b[off++] = '/';
  }
  if (!*buf)
    return 0;
  b = *buf;
  b->b[off - 1] = 0;
  n = b->used - 1;
  i = (n >> 1) - 1;
  for (; i >= 0; --i) {
    t = b->b[i];
    b->b[i] = b->b[n - i - 1];
    b->b[n - i - 1] = t;
  }
  return 0;
}
