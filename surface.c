#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "9p.h"
#include "fs.h"
#include "fsutil.h"
#include "surface.h"
#include "util.h"

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
}

static void
size_read(struct p9_connection *c)
{
  struct surface *s = get_surface(c);
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

struct surface *
mk_surface(int w, int h)
{
  struct surface *s;
  int pixelsize = 4;

  s = (struct surface *)malloc(sizeof(struct surface));
  if (!s)
    die("cannot allocate memory");
  memset(s, 0, sizeof(*s));
  s->w = w;
  s->h = h;
  s->pixels = (int *)malloc(w * h * pixelsize);
  if (!s->pixels)
    die("cannot allocate memory");
  /* TODO: create imlib surface */

  s->f.mode = P9_DMDIR | 0500;
  s->f.qpath = ++qid_cnt;
  s->f.context.p = s;
  s->f.rm = rm_surface;

  s->fsize.name = "size";
  s->fsize.mode = 0600;
  s->fsize.qpath = ++qid_cnt;
  s->fsize.fs = &surface_size_fs;
  s->fsize.context.p = s;
  add_file(&s->fsize, &s->f);

  s->fpixels.name = "pixels";
  s->fpixels.mode = 0600;
  s->fpixels.qpath = ++qid_cnt;
  s->fpixels.fs = &surface_pixels_fs;
  s->fpixels.context.p = s;
  s->fpixels.length = w * h * pixelsize;
  add_file(&s->fpixels, &s->f);

  return s;
}
