#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "util.h"
#include "9p.h"
#include "fs.h"
#include "fsutil.h"
#include "surface.h"

static struct surface *
get_surface(struct p9_connection *c)
{
  struct p9_fid *fid = (struct p9_fid *)c->t.context;
  struct file *f = (struct file *)fid->context;
  return (struct surface *)f->context.p;
}

static void
size_open(struct p9_connection *c)
{
  struct surface *s = get_surface(c);

  snprintf(s->size_buf, sizeof(s->size_buf), "%u %u", s->w, s->h);

  log_printf(3, "surface_size_open s: %p buf: '%.*s'\n", s,
             sizeof(s->size_buf), s->size_buf);
}

static void
size_read(struct p9_connection *c)
{
  struct surface *s = get_surface(c);
  log_printf(3, "surface_size_read s: %p buf: '%.*s'\n", s,
             sizeof(s->size_buf), s->size_buf);
  read_buf_fn(c, strlen(s->size_buf), s->size_buf);
}

static void
size_write(struct p9_connection *c)
{
  struct surface *s = get_surface(c);
  write_buf_fn(c, sizeof(s->size_buf), s->size_buf);
}

static void
size_clunk(struct p9_connection *c)
{
  struct surface *s = get_surface(c);
  unsigned int w, h;
  unsigned char *pixels = 0;

  log_printf(3, "surface_size_clunk s: %p buf: '%.*s'\n", s,
             sizeof(s->size_buf), s->size_buf);
  if (sscanf(s->size_buf, "%u %u", &w, &h) != 2) {
    P9_SET_STR(c->r.ename, "Wrong image file format");
    return;
  }
  if (w == s->w && h == s->h)
    return;
  /* TODO: resize image */
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
  struct surface *s = (struct surface *)f->context.p;
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
    die("cannot allocate memory");
  /* TODO: create imlib surface */

  s->fs.mode = 0500 | P9_DMDIR;
  s->fs.qpath = ++qid_cnt;
  s->fs.context.p = s;
  s->fs.rm = rm_surface;

  s->fs_size.name = "size";
  s->fs_size.mode = 0600;
  s->fs_size.qpath = ++qid_cnt;
  s->fs_size.fs = &surface_size_fs;
  s->fs_size.context.p = s;
  add_file(&s->fs, &s->fs_size);

  s->fs_pixels.name = "pixels";
  s->fs_pixels.mode = 0600;
  s->fs_pixels.qpath = ++qid_cnt;
  s->fs_pixels.fs = &surface_pixels_fs;
  s->fs_pixels.context.p = s;
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
    die("cannot allocate memory");
  if (init_surface(s, w, h)) {
    free(s);
    return 0;
  }
  return s;
}
