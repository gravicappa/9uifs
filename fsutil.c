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
