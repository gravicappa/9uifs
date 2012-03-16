#include <string.h>
#include "9p.h"

void
read_buf_fn(struct p9_connection *c, int size, char *buf)
{
  unsigned int n;

  if (c->t.offset >= size) {
    P9_SET_STR(c->r.ename, "Illegal seek");
    return;
  }
  c->r.count = c->t.count;
  if (c->t.offset + c->t.count > size)
    c->r.count = size - c->t.count;
  c->r.data = buf + c->t.offset;
}

void
write_buf_fn(struct p9_connection *c, int size, char *buf)
{
  unsigned int n;

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
  if (c->t.offset > 1) {
    P9_SET_STR(c->r.ename, "Illegal seek");
    return;
  }
  c->r.count = 1;
  c->r.data = val ? "1" : "0";
}

int
write_bool_fn(struct p9_connection *c, int oldval)
{
  if (c->r.count == 0)
    return oldval;
  if (c->r.count == 1)
    return c->r.data[0] != '0';
  if (c->r.count > 1)
    return !(c->r.data[0] == '0' && strchr("\n\t\r ", c->r.data[1]));
}
