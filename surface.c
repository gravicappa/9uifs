#include <stdlib.h>
#include <string.h>
#include <stdio.h>
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

static struct ctl_cmd ctl_cmd[] = {
  {"blit", cmd_blit},
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
size_open(struct p9_connection *c)
{
  struct p9_fid *fid = c->t.pfid;
  struct surface *s = (struct surface *)((struct file *)fid->file)->parent;
  int mode = c->t.mode;

  fid->aux = calloc(1, size_buf_len);
  if (!fid->aux) {
    P9_SET_STR(c->r.ename, "out of memory");
    return;
  }
  log_printf(LOG_DBG, "surface/size open p: %p mode: %x size: [%d %d]\n",
             s, mode, s->w, s->h);
  log_printf(LOG_DBG, "             trunc: %d\n", mode & P9_OTRUNC);
  log_printf(LOG_DBG, "             rdwr: %d\n", mode & P9_ORDWR);
  log_printf(LOG_DBG, "             read: %d\n", mode & P9_OREAD);
  if (!(mode & P9_OTRUNC) || (mode & P9_ORDWR) || (mode & P9_OREAD))
    snprintf((char *)fid->aux, size_buf_len, "%u %u", s->w, s->h);
  fid->rm = buf_fid_rm;
}

static void
size_read(struct p9_connection *c)
{
  struct p9_fid *fid = c->t.pfid;
  read_data_fn(c, strlen((char *)fid->aux), (char *)fid->aux);
}

static void
size_write(struct p9_connection *c)
{
  struct p9_fid *fid = c->t.pfid;
  write_data_fn(c, size_buf_len - 1, (char *)fid->aux);
}

static void
size_clunk(struct p9_connection *c)
{
  struct p9_fid *fid = c->t.pfid;
  struct surface *s = (struct surface *)((struct file *)fid->file)->parent;
  unsigned int w, h;
  int mode = c->t.pfid->open_mode;

  if (!(fid->aux && s->img
      && ((mode & 3) == P9_OWRITE || (mode & 3) == P9_ORDWR)))
    return;

  if (sscanf((char *)fid->aux, "%u %u", &w, &h) != 2) {
    P9_SET_STR(c->r.ename, "Wrong image file format");
    return;
  }
  if (s->w != w || s->h != h) {
    if (resize_surface(s, w, h)) {
      P9_SET_STR(c->r.ename, "Cannot resize blit surface");
      return;
    }
    if (s->update)
      s->update(s);
  }
}

static void
pixels_open(struct p9_connection *c)
{
  struct p9_fid *fid = c->t.pfid;
  struct surface *s = (struct surface *)((struct file *)fid->file)->parent;

  if (!s->img) {
    P9_SET_STR(c->r.ename, "no image");
    return;
  }
  s->flags &= ~SURFACE_DIRTY;
}

static void
pixels_read(struct p9_connection *c)
{
  struct surface *s = get_surface(c);
  char *pix;

  if (!s->img)
    return;
  pix = (char *)image_get_data(s->img, 0);
  read_data_fn(c, s->w * s->h * 4, pix);
}

static void
pixels_write(struct p9_connection *c)
{
  struct surface *s = get_surface(c);
  char *pix;

  if (!s->img)
    return;
  pix = (char *)image_get_data(s->img, 1);
  write_data_fn(c, s->w * s->h * 4, pix);
  image_put_back_data(s->img, pix);
  s->flags |= SURFACE_DIRTY;
}

static void
pixels_clunk(struct p9_connection *c)
{
  struct surface *s = get_surface(c);
  if (s->update && (s->flags & SURFACE_DIRTY))
    s->update(s);
  s->flags &= ~SURFACE_DIRTY;
}

static void
png_open(struct p9_connection *c)
{
  struct p9_fid *fid = c->t.pfid;
  fid->aux = 0;
  fid->rm = buf_fid_rm;
}

static void
png_write(struct p9_connection *c)
{
  struct p9_fid *fid = c->t.pfid;
  write_buf_fn(c, 256, (struct arr **)&fid->aux);
}

static void
png_clunk(struct p9_connection *c)
{
  struct surface *s = get_surface(c);
  struct arr *buf = c->t.pfid->aux;
  unsigned char *pixels, *p;
  int w, h, n, i, t;
  UImage newimg;

  if (!(buf && buf->used))
    return;
  pixels = stbi_load_from_memory((unsigned char *)buf->b, buf->used, &w, &h,
                                 &n, 4);
  if (!pixels)
    return;
  for (i = w * h, p = pixels; i; --i, p += 4) {
    t = p[0];
    p[0] = p[2];
    p[1] = p[1];
    p[2] = t;
  }
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
init_surface(struct surface *s, int w, int h)
{
  memset(s, 0, sizeof(*s));
  s->w = w;
  s->h = h;

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

  s->f_pixels.name = "pixels";
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

  log_printf(LOG_DBG, "init_surface %d %d p: %p\n", w, h, s);
  return 0;
}

struct surface *
mk_surface(int w, int h)
{
  struct surface *s;

  s = (struct surface *)malloc(sizeof(struct surface));
  if (s && init_surface(s, w, h)) {
    free(s);
    return 0;
  }
  return s;
}

static void
cmd_blit(struct file *f, char *cmd)
{
  log_printf(LOG_DBG, "#surface/ctl blit\n");
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
