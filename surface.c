#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "util.h"
#include "9p.h"
#include "fs.h"
#include "fsutil.h"
#include "surface.h"

const int size_buf_len = 32;

static struct surface *
get_surface(struct p9_connection *c)
{
  struct p9_fid *fid = c->t.pfid;
  struct file *f = (struct file *)fid->file;
  return (struct surface *)f->aux.p;
}

static void
size_fid_rm(struct p9_fid *fid)
{
  log_printf(3, "surface_size_fid_rm buf: '%p'\n", fid->aux);
  if (fid->aux) {
    free(fid->aux);
    fid->aux = 0;
  }
  fid->rm = 0;
}

static void
size_open(struct p9_connection *c)
{
  struct surface *s = get_surface(c);
  struct p9_fid *fid = c->t.pfid;

  fid->aux = malloc(size_buf_len);
  if (!fid->aux) {
    P9_SET_STR(c->r.ename, "out of memory");
    return;
  }
  memset(fid->aux, 0, size_buf_len);
  if (!(c->t.mode & P9_OTRUNC))
    snprintf((char *)fid->aux, size_buf_len, "%u %u", s->w, s->h);
  fid->rm = size_fid_rm;

  log_printf(3, "surface_size_open %p buf: '%.*s'\n", fid, size_buf_len,
             (char *)fid->aux);
}

static void
size_read(struct p9_connection *c)
{
  struct p9_fid *fid = c->t.pfid;
  log_printf(3, "surface_size_read %p buf: '%.*s'\n", fid, size_buf_len,
             (char *)fid->aux);
  read_buf_fn(c, strlen((char *)fid->aux), (char *)fid->aux);
}

static void
size_write(struct p9_connection *c)
{
  struct p9_fid *fid = c->t.pfid;
  log_printf(3, "surface_size_write %p buf: '%.*s'\n", fid, size_buf_len,
             (char *)fid->aux);
  write_buf_fn(c, size_buf_len - 1, (char *)fid->aux);
}

static void
size_clunk(struct p9_connection *c)
{
  struct surface *s = get_surface(c);
  struct p9_fid *fid = c->t.pfid;
  unsigned int w, h;
  char *pixels = 0, *buf;

  if (!fid->aux)
    return;

  if (sscanf((char *)fid->aux, "%u %u", &w, &h) != 2) {
    P9_SET_STR(c->r.ename, "Wrong image file format");
    return;
  }
  if (w == s->w && h == s->h)
    return;
  /* TODO: resize image */
  s->w = w;
  s->h = h;
}

static void
pixels_open(struct p9_connection *c)
{
  struct surface *s = get_surface(c);
}

static void
pixels_read(struct p9_connection *c)
{
  struct surface *s = get_surface(c);
  read_buf_fn(c, s->w * s->h * 4, (char *)s->pixels);
}

static void
pixels_write(struct p9_connection *c)
{
  struct surface *s = get_surface(c);
  write_buf_fn(c, s->w * s->h * 4, (char *)s->pixels);
}

static void
pixels_clunk(struct p9_connection *c)
{
  struct surface *s = get_surface(c);
}

static struct p9_fs surface_size_fs = {
  .open = size_open,
  .read = size_read,
  .write = size_write,
  .clunk = size_clunk
};

static struct p9_fs surface_pixels_fs = {
  .open = pixels_open,
  .read = pixels_read,
  .write = pixels_write,
  .clunk = pixels_clunk,
};

void
rm_surface(struct file *f)
{
  struct surface *s = (struct surface *)f->aux.p;
  /* TODO: free surface */
}

int
init_surface(struct surface *s, int w, int h)
{
  int pixelsize = 4;

  memset(s, 0, sizeof(*s));
  s->w = w;
  s->h = h;
  s->pixels = (int *)malloc(w * h * pixelsize);
  if (!s->pixels)
    die("Cannot allocate memory");
  /* TODO: create imlib surface */

  s->fs.mode = 0500 | P9_DMDIR;
  s->fs.qpath = ++qid_cnt;
  s->fs.aux.p = s;
  s->fs.rm = rm_surface;

  s->fs_size.name = "size";
  s->fs_size.mode = 0600;
  s->fs_size.qpath = ++qid_cnt;
  s->fs_size.fs = &surface_size_fs;
  s->fs_size.aux.p = s;
  add_file(&s->fs, &s->fs_size);

  s->fs_pixels.name = "pixels";
  s->fs_pixels.mode = 0600;
  s->fs_pixels.qpath = ++qid_cnt;
  s->fs_pixels.fs = &surface_pixels_fs;
  s->fs_pixels.aux.p = s;
  s->fs_pixels.length = w * h * pixelsize;
  add_file(&s->fs, &s->fs_pixels);

  return 0;
}

struct surface *
mk_surface(int w, int h)
{
  struct surface *s;

  s = (struct surface *)malloc(sizeof(struct surface));
  if (!s)
    die("Cannot allocate memory");
  if (init_surface(s, w, h)) {
    free(s);
    return 0;
  }
  return s;
}
