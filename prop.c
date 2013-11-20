#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "util.h"
#include "9p.h"
#include "fs.h"
#include "fsutil.h"
#include "fstypes.h"
#include "prop.h"
#include "frontend.h"
#include "raster.h"

#define INT_BUF_SIZE 16
#define RECT_BUF_SIZE 64

void
prop_clunk(struct p9_connection *con)
{
  struct prop *p = (struct prop *)con->t.pfid->file;

  if (p && p->update && P9_WRITE_MODE(con->t.pfid->open_mode))
    p->update(p);
}

static int
prop_alloc(struct p9_connection *con, int size)
{
  struct p9_fid *fid = con->t.pfid;

  fid->aux = calloc(1, size);
  if (!fid->aux)
    return -1;
  fid->rm = rm_fid_aux;
  return 0;
}

void
prop_int_open(struct p9_connection *con, int size, const char *fmt)
{
  struct prop_int *p;
  struct p9_fid *fid = con->t.pfid;

  p = (struct prop_int *)fid->file;
  if (prop_alloc(con, size))
    die("Cannot allocate memory");
  if (P9_WRITE_MODE(con->t.mode) && !(con->t.mode & P9_OTRUNC))
    snprintf((char *)fid->aux, size, fmt, p->i);
}

int
prop_int_clunk(struct p9_connection *con, const char *fmt)
{
  int x;
  struct prop_int *p;

  if (!con->t.pfid->aux)
    return 0;

  if (P9_WRITE_MODE(con->t.pfid->open_mode)) {
    p = (struct prop_int *)con->t.pfid->file;
    if (sscanf((char *)con->t.pfid->aux, fmt, &x) != 1)
      return -1;
    if (p)
      p->i = x;
  }
  prop_clunk(con);
  return 0;
}

void
prop_intdec_open(struct p9_connection *con)
{
  prop_int_open(con, INT_BUF_SIZE, "%d");
}

void
prop_intdec_read(struct p9_connection *con)
{
  char *s = (char *)con->t.pfid->aux;
  read_data_fn(con, strlen(s), s);
}

void
prop_intdec_write(struct p9_connection *con)
{
  write_data_fn(con, INT_BUF_SIZE - 1, (char *)con->t.pfid->aux);
}

void
prop_intdec_clunk(struct p9_connection *con)
{
  if (prop_int_clunk(con, "%d"))
    P9_SET_STR(con->r.ename, "Wrong number format");
}

static void
prop_buf_rm(struct file *f)
{
  struct prop_buf *p;
  p = (struct prop_buf *)f;
  if (p && p->buf) {
    free(p->buf);
    p->buf = 0;
  }
}

void
prop_buf_open(struct p9_connection *con)
{
  struct prop_buf *p = (struct prop_buf *)con->t.pfid->file;
  if (P9_WRITE_MODE(con->t.mode) && (con->t.mode & P9_OTRUNC) && p->buf) {
    p->buf->used = 0;
    memset(p->buf->b, 0, p->buf->size);
  }
}

void
prop_buf_read(struct p9_connection *con)
{
  struct prop_buf *p = (struct prop_buf *)con->t.pfid->file;
  if (p->buf)
    read_str_fn(con, p->buf->used, p->buf->b);
}

void
prop_fixed_buf_write(struct p9_connection *con)
{
  struct prop_buf *p = (struct prop_buf *)con->t.pfid->file;
  if (p->buf) {
    write_data_fn(con, p->buf->size - 1, p->buf->b);
    p->buf->used = strlen(p->buf->b);
  }
}

void
prop_buf_write(struct p9_connection *con)
{
  struct prop_buf *p = (struct prop_buf *)con->t.pfid->file;
  write_buf_fn(con, 16, &p->buf);
}

void
prop_colour_open(struct p9_connection *con)
{
  int size = 9, r, g, b, a;
  struct p9_fid *fid = con->t.pfid;
  struct prop_int *p;

  if (prop_alloc(con, size)) {
    P9_SET_STR(con->r.ename, "Cannot allocate memory");
    return;
  }
  if (!(con->t.mode & P9_OTRUNC) && P9_READ_MODE(con->t.mode)) {
    p = (struct prop_int *)fid->file;
    r = RGBA_R(p->i);
    g = RGBA_G(p->i);
    b = RGBA_B(p->i);
    a = RGBA_A(p->i);
    snprintf((char *)fid->aux, size, "%02x%02x%02x%02x", r, g, b, a);
  }
}

void
prop_colour_read(struct p9_connection *con)
{
  read_data_fn(con, 8, (char *)con->t.pfid->aux);
}

void
prop_colour_write(struct p9_connection *con)
{
  write_data_fn(con, 8, (char *)con->t.pfid->aux);
}

void
prop_colour_clunk(struct p9_connection *con)
{
  unsigned int n, r = 0, g = 0, b = 0, a = 0xff, len;
  struct prop_int *p;
  struct p9_fid *fid = con->t.pfid;
  char *buf = (char *)fid->aux;

  p = (struct prop_int *)fid->file;

  if (!buf)
    return;

  if (P9_WRITE_MODE(fid->open_mode)) {
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
    if (p)
      p->i = RGBA(r, g, b, a);
  }
  prop_clunk(con);
  return;
error:
    P9_SET_STR(con->r.ename, "Wrong colour format");
}

void
rect_open(struct p9_connection *con)
{
  struct prop_rect *p;
  struct p9_fid *fid = con->t.pfid;

  p = (struct prop_rect *)fid->file;
  fid->aux = calloc(1, RECT_BUF_SIZE);
  fid->rm = rm_fid_aux;
  if (P9_READ_MODE(con->t.mode) && !(con->t.mode & P9_OTRUNC))
    snprintf((char *)fid->aux, RECT_BUF_SIZE, "%d %d %d %d", p->r[0], p->r[1],
             p->r[2], p->r[3]);
}

void
rect_read(struct p9_connection *con)
{
  char *s = (char *)con->t.pfid->aux;
  read_data_fn(con, strlen(s), s);
}

void
rect_write(struct p9_connection *con)
{
  write_data_fn(con, RECT_BUF_SIZE - 1, (char *)con->t.pfid->aux);
}

void
rect_clunk_fn(struct p9_connection *con, int deffill)
{
  int r[4], i, n;
  struct prop_rect *p;
  char *s = (char *)con->t.pfid->aux;

  if (!(s && P9_WRITE_MODE(con->t.pfid->open_mode)))
    return;

  p = (struct prop_rect *)con->t.pfid->file;
  n = sscanf(s, "%d %d %d %d", &r[0], &r[1], &r[2], &r[3]);
  if (deffill == -1 && n != 4)
    return;
  if (p) {
    for (i = 0; i < 4 && i < n; ++i)
      p->r[i] = r[i];
    for (; i < 4; ++i)
      p->r[i] = deffill;
    if (p->p.update)
      p->p.update(&p->p);
  }
}

void
rect_clunk(struct p9_connection *con)
{
  rect_clunk_fn(con, -1);
}

void
rect_defzero_clunk(struct p9_connection *con)
{
  rect_clunk_fn(con, 0);
}

void
intarr_open(struct p9_connection *con)
{
  struct prop_intarr *p;
  struct p9_fid *fid = con->t.pfid;
  int i, n, off, *arr, len;
  char buf[16], *sep;
  struct arr *a = 0;

  fid->rm = rm_fid_aux;
  fid->aux = 0;

  p = (struct prop_intarr *)fid->file;
  if (P9_WRITE_MODE(con->t.mode) && (con->t.mode & P9_OTRUNC))
    return;

  off = 0;
  arr = p->arr;
  n = p->n;

  for (i = 0, sep = ""; i < n; ++i, ++arr) {
    len = snprintf(buf, sizeof(buf), "%s%d", sep, *arr);
    if (arr_memcpy(&a, 16, off, len + 1, buf) < 0) {
      P9_SET_STR(con->r.ename, "out of memory");
      return;
    }
    off += len;
    sep = " ";
  }
  fid->aux = a;
}

void
intarr_read(struct p9_connection *con)
{
  struct arr *a = (struct arr *)con->t.pfid->aux;
  if (a)
    read_data_fn(con, strlen(a->b), (char *)a->b);
}

void
intarr_write(struct p9_connection *con)
{
  write_buf_fn(con, 16, (struct arr **)&con->t.pfid->aux);
}

void
intarr_clunk(struct p9_connection *con)
{
  struct arr *arr = (struct arr *)con->t.pfid->aux;
  int i, n, *ptr, x;
  struct prop_intarr *p = (struct prop_intarr *)con->t.pfid->file;
  char *s, *a;

  if (!(arr && p && p->arr && p->n && P9_WRITE_MODE(con->t.pfid->open_mode)))
    return;
  n = p->n;
  ptr = p->arr;
  s = arr->b;

  for (i = 0, a = next_arg(&s); i < n && a; ++i, a = next_arg(&s), ++ptr)
    if (sscanf(a, "%d", &x) == 1)
      *ptr = x;

  if (p->p.update)
    p->p.update(&p->p);
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

struct p9_fs rect_defzero_fs = {
  .open = rect_open,
  .read = rect_read,
  .write = rect_write,
  .clunk = rect_defzero_clunk
};

struct p9_fs intarr_fs = {
  .open = intarr_open,
  .read = intarr_read,
  .write = intarr_write,
  .clunk = intarr_clunk
};

static void
init_prop_fs(struct prop *p, char *name, void *aux)
{
  p->aux = aux;
  p->f.name = name;
  p->f.mode = 0600;
  p->f.qpath = new_qid(FS_PROP);
}

int
init_prop_int(struct file *root, struct prop_int *p, char *name, int x,
              void *aux)
{
  memset(p, 0, sizeof(*p));
  init_prop_fs(&p->p, name, aux);
  p->i = x;
  p->p.f.fs = &int_fs;
  add_file(root, &p->p.f);
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
  p->p.f.fs = &buf_fs;
  if (fixed_size) {
    p->p.f.fs = &fixed_buf_fs;
    p->buf->used = size;
  }
  p->p.f.fs = (fixed_size) ? &fixed_buf_fs : &buf_fs;
  p->p.f.rm = prop_buf_rm;
  add_file(root, &p->p.f);
  return 0;
}

int
init_prop_colour(struct file *root, struct prop_int *p, char *name,
                 unsigned int rgba, void *aux)
{
  memset(p, 0, sizeof(*p));
  init_prop_fs(&p->p, name, aux);
  p->i = rgba;
  p->p.f.fs = &colour_fs;
  add_file(root, &p->p.f);
  return 0;
}

int
init_prop_rect(struct file *root, struct prop_rect *p, char *name,
               int defzero, void *aux)
{
  memset(p, 0, sizeof(*p));
  init_prop_fs(&p->p, name, aux);
  p->p.f.fs = (defzero) ? &rect_defzero_fs : &rect_fs;
  add_file(root, &p->p.f);
  return 0;
}

int
init_prop_intarr(struct file *root, struct prop_intarr *p, char *name, int n,
                 int *arr, void *aux)
{
  memset(p, 0, sizeof(*p));
  init_prop_fs(&p->p, name, aux);
  p->n = n;
  p->arr = arr;
  p->p.f.fs = &intarr_fs;
  add_file(root, &p->p.f);
  return 0;
}
