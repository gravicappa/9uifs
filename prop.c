#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "util.h"
#include "9p.h"
#include "fs.h"
#include "fsutil.h"
#include "fstypes.h"
#include "prop.h"

#define INT_BUF_SIZE 16
#define RECT_BUF_SIZE 64

static void
aux_free(struct p9_fid *fid)
{
  if (fid->aux)
    free(fid->aux);
  fid->rm = 0;
}

void
prop_clunk(struct p9_connection *c)
{
  struct prop *p = (struct prop *)c->t.pfid->file;
  int mode = c->t.pfid->open_mode;

  if (p && p->update && ((mode & 3) == P9_OWRITE || (mode & 3) == P9_ORDWR))
    p->update(p);
}

static int
prop_alloc(struct p9_connection *c, int size)
{
  struct p9_fid *fid = c->t.pfid;

  fid->aux = malloc(size);
  if (!fid->aux)
    return -1;
  memset(fid->aux, 0, size);
  fid->rm = aux_free;
  return 0;
}

void
prop_int_open(struct p9_connection *c, int size, const char *fmt)
{
  struct prop_int *p;
  struct p9_fid *fid = c->t.pfid;

  p = (struct prop_int *)fid->file;
  if (prop_alloc(c, size))
    die("Cannot allocate memory");
  if (!(c->t.mode & P9_OTRUNC))
    snprintf((char *)fid->aux, size, fmt, p->i);
}

int
prop_int_clunk(struct p9_connection *c, const char *fmt)
{
  int x;
  struct prop_int *p;
  int mode = c->t.pfid->open_mode;

  if (!c->t.pfid->aux)
    return 0;

  p = (struct prop_int *)c->t.pfid->file;
  if (sscanf((char *)c->t.pfid->aux, fmt, &x) != 1)
    return -1;
  if (p && ((mode & 3) == P9_OWRITE || (mode & 3) == P9_ORDWR))
    p->i = x;
  prop_clunk(c);
  return 0;
}

void
prop_intdec_open(struct p9_connection *c)
{
  prop_int_open(c, INT_BUF_SIZE, "%d");
}

void
prop_intdec_read(struct p9_connection *c)
{
  read_buf_fn(c, strlen((char *)c->t.pfid->aux), (char *)c->t.pfid->aux);
}

void
prop_intdec_write(struct p9_connection *c)
{
  write_buf_fn(c, INT_BUF_SIZE - 1, (char *)c->t.pfid->aux);
}

void
prop_intdec_clunk(struct p9_connection *c)
{
  if (prop_int_clunk(c, "%d"))
    P9_SET_STR(c->r.ename, "Wrong number format");
}

static void
prop_buf_rm(struct file *f)
{
  struct prop_buf *p;
  p = (struct prop_buf *)f->aux.p;
  if (p && p->buf) {
    free(p->buf);
    p->buf = 0;
  }
}

void
prop_buf_open(struct p9_connection *c)
{
  struct prop_buf *p = (struct prop_buf *)c->t.pfid->file;
  if ((c->t.mode & P9_OTRUNC) && p->buf) {
    p->buf->used = 0;
    memset(p->buf->b, 0, p->buf->size);
  }
}

void
prop_buf_read(struct p9_connection *c)
{
  struct prop_buf *p = (struct prop_buf *)c->t.pfid->file;
  int i;
  char *s;
  if (p->buf) {
    for (s = p->buf->b, i = 0; i < p->buf->used && *s; ++i, ++s) {}
    read_buf_fn(c, i, p->buf->b);
  }
}

void
prop_fixed_buf_write(struct p9_connection *c)
{
  struct prop_buf *p = (struct prop_buf *)c->t.pfid->file;
  if (p->buf) {
    write_buf_fn(c, p->buf->size - 1, p->buf->b);
    p->buf->used = strlen(p->buf->b);
  }
}

void
prop_buf_write(struct p9_connection *c)
{
  struct prop_buf *p = (struct prop_buf *)c->t.pfid->file;
  int off, u;

  u = (p->buf) ? p->buf->used : 0;
  off = arr_memcpy(&p->buf, 32, c->t.offset, c->t.count + 1, 0);
  if (off < 0) {
    P9_SET_STR(c->r.ename, "Cannot allocate memory");
    return;
  }
  memset(p->buf->b + u, 0, p->buf->used - u);
  write_buf_fn(c, p->buf->size - 1, p->buf->b);
}

void
prop_colour_open(struct p9_connection *c)
{
  int size = 9, r, g, b, a;
  struct p9_fid *fid = c->t.pfid;
  struct prop_int *p;

  if (prop_alloc(c, size)) {
    P9_SET_STR(c->r.ename, "Cannot allocate memory");
    return;
  }
  if (!(c->t.mode & P9_OTRUNC)) {
    p = (struct prop_int *)fid->file;
    r = (p->i >> 16) & 0xff;
    g = (p->i >> 8) & 0xff;
    b = (p->i) & 0xff;
    a = (p->i >> 24) & 0xff;
    snprintf((char *)fid->aux, size, "%02x%02x%02x%02x", r, g, b, a);
  }
}

void
prop_colour_read(struct p9_connection *c)
{
  read_buf_fn(c, 8, (char *)c->t.pfid->aux);
}

void
prop_colour_write(struct p9_connection *c)
{
  write_buf_fn(c, 8, (char *)c->t.pfid->aux);
}

void
prop_colour_clunk(struct p9_connection *c)
{
  char *buf = (char *)c->t.pfid->aux;
  unsigned int n, r = 0, g = 0, b = 0, a = 0xff, len;
  struct prop_int *p;
  struct p9_fid *fid = c->t.pfid;
  int mode = c->t.pfid->open_mode;

  p = (struct prop_int *)fid->file;

  if (!buf)
    return;

  len = strlen(buf);
  if (len <= 4) {
    n = sscanf(buf, "%01x%01x%01x%01x", &r, &g, &b, &a);
    if (n < 3)
      goto error;
    r <<= 4;
    g <<= 4;
    b <<= 4;
    a = (n == 4) ? (a << 4) : 0xff;
  } else if (len <= 8) {
    n = sscanf(buf, "%02x%02x%02x%02x", &r, &g, &b, &a);
    if (n < 3)
      goto error;
    if (n < 4)
      a = 0xff;
  } else
    goto error;

  if (p && ((mode & 3) == P9_OWRITE || (mode & 3) == P9_ORDWR))
    p->i = (a << 24) | (r << 16) | (g << 8) | b;
  prop_clunk(c);
  return;
error:
    P9_SET_STR(c->r.ename, "Wrong colour format");
}

void
rect_open(struct p9_connection *c)
{
  struct prop_rect *p;
  struct p9_fid *fid = c->t.pfid;

  p = (struct prop_rect *)fid->file;
  fid->aux = malloc(RECT_BUF_SIZE);
  memset(fid->aux, 0, RECT_BUF_SIZE);
  fid->rm = aux_free;
  if (!(c->t.mode & P9_OTRUNC))
    snprintf((char *)fid->aux, RECT_BUF_SIZE, "%d %d %d %d", p->r[0], p->r[1],
             p->r[2], p->r[3]);
  else
    memset(fid->aux, 0, RECT_BUF_SIZE);
}

void
rect_read(struct p9_connection *c)
{
  read_buf_fn(c, strlen((char *)c->t.pfid->aux), (char *)c->t.pfid->aux);
}

void
rect_write(struct p9_connection *c)
{
  write_buf_fn(c, RECT_BUF_SIZE - 1, (char *)c->t.pfid->aux);
}

void
rect_clunk(struct p9_connection *c)
{
  int r[4];
  struct prop_rect *p;
  char *s = (char *)c->t.pfid->aux;

  if (!s)
    return;

  p = (struct prop_rect *)c->t.pfid->file;
  if (sscanf(s, "%d %d %d %d", &r[0], &r[1], &r[2], &r[3]) != 4)
    return;
  if (p) {
    memcpy(p->r, r, sizeof(r));
    if (p->p.update)
      p->p.update(&p->p);
  }
}

struct p9_fs int_fs = {
  .open = prop_intdec_open,
  .read = prop_intdec_read,
  .write = prop_intdec_write,
  .clunk = prop_intdec_clunk
};

struct p9_fs fixed_buf_fs = {
  .open = prop_buf_open,
  .read = prop_buf_read,
  .write = prop_fixed_buf_write,
  .clunk = prop_clunk
};

struct p9_fs buf_fs = {
  .open = prop_buf_open,
  .read = prop_buf_read,
  .write = prop_buf_write,
  .clunk = prop_clunk
};

struct p9_fs colour_fs = {
  .open = prop_colour_open,
  .read = prop_colour_read,
  .write = prop_colour_write,
  .clunk = prop_colour_clunk
};

struct p9_fs rect_fs = {
  .open = rect_open,
  .read = rect_read,
  .write = rect_write,
  .clunk = rect_clunk
};

static void
init_prop_fs(struct prop *p, char *name, void *aux)
{
  p->aux = aux;
  p->fs.name = name;
  p->fs.mode = 0600;
  p->fs.qpath = new_qid(FS_PROP);
  p->fs.aux.p = p;
}

int
init_prop_int(struct file *root, struct prop_int *p, char *name, int x,
              void *aux)
{
  memset(p, 0, sizeof(*p));
  init_prop_fs(&p->p, name, aux);
  p->i = x;
  p->p.fs.fs = &int_fs;
  add_file(root, &p->p.fs);
  return 0;
}

int
init_prop_buf(struct file *root, struct prop_buf *p, char *name, int size,
              char *x, int fixed_size, void *aux)
{
  init_prop_fs(&p->p, name, aux);
  if (arr_memcpy(&p->buf, 8, -1, size + 1, 0) < 0)
    return -1;
  if (x) {
    memcpy(p->buf->b, x, size);
    p->buf->b[size] = 0;
  } else
    memset(p->buf->b, 0, p->buf->size);
  p->p.fs.fs = &buf_fs;
  if (fixed_size) {
    p->p.fs.fs = &fixed_buf_fs;
    p->buf->used = size;
  }
  p->p.fs.fs = (fixed_size) ? &fixed_buf_fs : &buf_fs;
  p->p.fs.rm = prop_buf_rm;
  add_file(root, &p->p.fs);
  return 0;
}

int
init_prop_colour(struct file *root, struct prop_int *p, char *name,
                 unsigned int rgba, void *aux)
{
  memset(p, 0, sizeof(*p));
  init_prop_fs(&p->p, name, aux);
  p->i = rgba;
  p->p.fs.fs = &colour_fs;
  add_file(root, &p->p.fs);
  return 0;
}

int
init_prop_rect(struct file *root, struct prop_rect *p, char *name, void *aux)
{
  memset(p, 0, sizeof(*p));
  init_prop_fs(&p->p, name, aux);
  p->p.fs.fs = &rect_fs;
  add_file(root, &p->p.fs);
  return 0;
}
