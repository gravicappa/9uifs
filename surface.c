#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include "util.h"
#include "9p.h"
#include "fs.h"
#include "fsutil.h"
#include "fstypes.h"
#include "ctl.h"
#include "draw.h"
#include "surface.h"
#include "stb_image.h"

const int size_buf_len = 32;

enum surface_flags {
  SURFACE_DIRTY = 1
};

static void cmd_blit(struct file *f, char *cmd);
static void cmd_rect(struct file *f, char *cmd);
static void cmd_line(struct file *f, char *cmd);
static void cmd_poly(struct file *f, char *cmd);
static void cmd_text(struct file *f, char *cmd);

static struct ctl_cmd ctl_cmd[] = {
  {"blit", cmd_blit},
  {"rect", cmd_rect},
  {"line", cmd_line},
  {"poly", cmd_poly},
  {"text", cmd_text},
  {0, 0}
};

static struct surface *
get_surface(struct p9_connection *c)
{
  struct p9_fid *fid = c->t.pfid;
  return (struct surface *)((struct file *)fid->file)->parent;
}

static void
buf_fid_rm(struct p9_fid *fid)
{
  if (fid->aux) {
    free(fid->aux);
    fid->aux = 0;
  }
  fid->rm = 0;
}

static void
size_open(struct p9_connection *con)
{
  struct p9_fid *fid = con->t.pfid;
  struct surface *s = (struct surface *)((struct file *)fid->file)->parent;

  fid->aux = calloc(1, size_buf_len);
  if (!fid->aux) {
    P9_SET_STR(con->r.ename, "out of memory");
    return;
  }
  if (!(con->t.mode & P9_OTRUNC) || P9_READ_MODE(con->t.mode))
    snprintf((char *)fid->aux, size_buf_len, "%u %u", s->w, s->h);
  fid->rm = buf_fid_rm;
}

static void
size_read(struct p9_connection *con)
{
  char *s = (char *)con->t.pfid->aux;
  read_data_fn(con, strlen(s), s);
}

static void
size_write(struct p9_connection *con)
{
  write_data_fn(con, size_buf_len - 1, (char *)con->t.pfid->aux);
}

static void
size_clunk(struct p9_connection *con)
{
  struct p9_fid *fid = con->t.pfid;
  struct surface *s = (struct surface *)((struct file *)fid->file)->parent;
  unsigned int w, h;

  if (!(fid->aux && s->img && P9_WRITE_MODE(fid->open_mode)))
    return;

  if (sscanf((char *)fid->aux, "%u %u", &w, &h) != 2) {
    P9_SET_STR(con->r.ename, "Wrong image file format");
    return;
  }
  if (s->w != w || s->h != h) {
    if (resize_surface(s, w, h)) {
      P9_SET_STR(con->r.ename, "Cannot resize blit surface");
      return;
    }
    if (s->update)
      s->update(s);
  }
}

static void
pixels_open(struct p9_connection *con)
{
  struct p9_fid *fid = con->t.pfid;
  struct surface *s = (struct surface *)((struct file *)fid->file)->parent;

  if (!s->img) {
    P9_SET_STR(con->r.ename, "no image");
    return;
  }
  s->flags &= ~SURFACE_DIRTY;
}

static void
pixels_read(struct p9_connection *con)
{
  struct surface *s = get_surface(con);
  unsigned int size;

  if (!s->img)
    return;
  size = s->w * s->h * 4;
  con->r.count = 0;
  if (con->t.offset >= size)
    return;
  con->r.count = con->t.count;
  if (con->t.offset + con->t.count > size)
    con->r.count = size - con->t.offset;
  image_read_rgba(s->img, con->t.offset, con->r.count, con->buf);
  con->r.data = con->buf;
}

static void
pixels_write(struct p9_connection *con)
{
  struct surface *s = get_surface(con);
  unsigned int size;

  if (!s->img)
    return;
  size = s->w * s->h * 4;
  con->r.count = con->t.count;
  if (con->t.offset >= size)
    return;
  if (con->t.offset + con->t.count > size)
    con->r.count = size - con->t.offset;
  image_write_rgba(s->img, con->t.offset, con->r.count, con->t.data);
  if (con->r.count)
    s->flags |= SURFACE_DIRTY;
}

static void
pixels_clunk(struct p9_connection *con)
{
  struct surface *s = get_surface(con);
  if (s->update && (s->flags & SURFACE_DIRTY))
    s->update(s);
  s->flags &= ~SURFACE_DIRTY;
}

static void
png_open(struct p9_connection *con)
{
  struct p9_fid *fid = con->t.pfid;
  fid->aux = 0;
  fid->rm = buf_fid_rm;
}

static void
png_write(struct p9_connection *con)
{
  struct p9_fid *fid = con->t.pfid;
  write_buf_fn(con, 512, (struct arr **)&fid->aux);
}

static void
png_clunk(struct p9_connection *con)
{
  struct surface *s = get_surface(con);
  struct arr *buf = con->t.pfid->aux;
  unsigned char *pixels;
  int w, h, n;
  UImage newimg;

  if (!(buf && buf->used))
    return;
  pixels = stbi_load_from_memory((unsigned char *)buf->b, buf->used, &w, &h,
                                 &n, 4);
  if (!pixels)
    return;
  newimg = create_image(w, h, pixels);
  free(pixels);
  if (!newimg)
    return;
  free_image(s->img);
  s->img = newimg;
  s->w = w;
  s->h = h;
  s->f_pixels.length = w * h * 4;
  if (s->update)
    s->update(s);
}

static struct p9_fs surface_size_fs = {
  .open = size_open,
  .read = size_read,
  .write = size_write,
  .clunk = size_clunk,
  .remove = size_clunk
};

static struct p9_fs surface_pixels_fs = {
  .open = pixels_open,
  .read = pixels_read,
  .write = pixels_write,
  .clunk = pixels_clunk,
};

static struct p9_fs surface_png_fs = {
  .open = png_open,
  .write = png_write,
  .clunk = png_clunk
};

void
rm_surface(struct file *f)
{
  struct surface *s = (struct surface *)f;
  if (s->img) {
    free_image(s->img);
    s->img = 0;
  }
}

int
init_surface(struct surface *s, int w, int h, struct file *imglib)
{
  memset(s, 0, sizeof(*s));
  s->w = w;
  s->h = h;
  s->imglib = imglib;

  s->img = create_image(w, h, 0);
  if (!s->img && w && h)
    return -1;

  s->f.mode = 0500 | P9_DMDIR;
  s->f.qpath = new_qid(FS_SURFACE);
  s->f.rm = rm_surface;

  s->f_ctl.f.name = "ctl";
  s->f_ctl.f.mode = 0200 | P9_DMAPPEND;
  s->f_ctl.f.qpath = new_qid(0);
  s->f_ctl.f.fs = &ctl_fs;
  s->f_ctl.cmd = ctl_cmd;
  add_file(&s->f, &s->f_ctl.f);

  s->f_size.name = "size";
  s->f_size.mode = 0600;
  s->f_size.qpath = new_qid(0);
  s->f_size.fs = &surface_size_fs;
  add_file(&s->f, &s->f_size);

  s->f_pixels.name = "rgba";
  s->f_pixels.mode = 0600;
  s->f_pixels.qpath = new_qid(0);
  s->f_pixels.fs = &surface_pixels_fs;
  s->f_pixels.length = w * h * 4;
  add_file(&s->f, &s->f_pixels);

  s->f_png.name = "png";
  s->f_png.mode = 0200;
  s->f_png.qpath = new_qid(0);
  s->f_png.fs = &surface_png_fs;
  add_file(&s->f, &s->f_png);
  return 0;
}

struct surface *
mk_surface(int w, int h, struct file *imglib)
{
  struct surface *s;

  s = (struct surface *)malloc(sizeof(struct surface));
  if (s && init_surface(s, w, h, imglib)) {
    free(s);
    return 0;
  }
  return s;
}

int
resize_surface(struct surface *s, int w, int h)
{
  UImage newimg;

  if (w == s->w && h == s->h)
    return 0;
  newimg = resize_image(s->img, w, h, 0);
  if (!newimg && w && h)
    return -1;
  s->img = newimg;
  s->w = w;
  s->h = h;
  s->f_pixels.length = w * h * 4;
  return 0;
}

static void
cmd_blit(struct file *f, char *cmd)
{
  int src_x = 0, src_y = 0, src_w, src_h, x = 0, y = 0, w, h;
  static const char *fmt = "%d %d %d %d %d %d %d %d";
  char *name;
  struct surface *src, *dst = containerof(f, struct surface, f_ctl);

  name = next_quoted_arg(&cmd);
  if (!name)
    return;
  src = (struct surface *)find_file(dst->imglib, strlen(name), name);
  if (!src)
    return;
  src_w = w = src->w;
  src_h = h = src->h;
  sscanf(cmd, fmt, &x, &y, &w, &h, &src_x, &src_y, &src_w, &src_h);
  blit_image(dst->img, x, y, w, h, src->img, src_x, src_y, src_w, src_h);

  if (dst->update)
    dst->update(dst);
}

static void
cmd_rect(struct file *f, char *cmd)
{
  int r[4], i, bg = 0, fg = 0xff000000;
  char *arg, *c = cmd;
  struct surface *s = containerof(f, struct surface, f_ctl);

  for (i = 0; i < 4; ++i)
    if (!(arg = next_arg(&c)) || sscanf(arg, "%d", &r[i]) != 1)
      return;
  if ((arg = next_arg(&c)))
    fg = RGBA_FROM_STR(arg);
  if ((arg = next_arg(&c)))
    bg = RGBA_FROM_STR(arg);

  draw_rect(s->img, r[0], r[1], r[2], r[3], fg, bg);
  if (s->update)
    s->update(s);
}

static void
cmd_line(struct file *f, char *cmd)
{
  int r[4], i, fg = 0xff000000;
  char *arg, *c = cmd;
  struct surface *s = containerof(f, struct surface, f_ctl);

  for (i = 0; i < 4; ++i)
    if (!(arg = next_arg(&c)) || sscanf(arg, "%d", &r[i]) != 1)
      return;
  if ((arg = next_arg(&c)))
    fg = RGBA_FROM_STR(arg);

  if (fg & 0xff000000)
    draw_line(s->img, r[0], r[1], r[2], r[3], fg);
  if (s->update)
    s->update(s);
}

#define STATIC_NPTS 32

static void
cmd_poly(struct file *f, char *cmd)
{
  char *arg, *c = cmd, *pc;
  struct surface *s = containerof(f, struct surface, f_ctl);
  int spts[STATIC_NPTS * 2];
  int i, n, npts, *pts = spts, *p, bg = 0, fg = 0xff000000;

  if (!(arg = next_arg(&c)))
    return;
  fg = RGBA_FROM_STR(arg);
  if (!(arg = next_arg(&c)))
    return;
  bg = RGBA_FROM_STR(arg);
  npts = nargs(c) >> 1;
  if (!npts)
    return;
  if (npts > 32) {
    pts = malloc(sizeof(int) * npts * 2);
    if (!pts) {
      /* out of memory. shoot out client */
      return;
    }
  }
  for (n = 0, p = pts, i = npts * 2; i; --i, ++p)
    if ((arg = next_arg(&c)) && sscanf(arg, "%d", p) == 1)
      ++n;
  if (n == npts * 2)
    draw_poly(s->img, npts, pts, fg, bg);
  if (pts != spts)
    free(pts);
}

static void
cmd_text(struct file *f, char *cmd)
{
  struct surface *s = containerof(f, struct surface, f_ctl);
}
