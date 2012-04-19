#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <Imlib2.h>
#include "util.h"
#include "9p.h"
#include "fs.h"
#include "fsutil.h"
#include "ctl.h"
#include "surface.h"

const int size_buf_len = 32;

static void cmd_blit(struct file *f, char *cmd);

static struct ctl_cmd ctl_cmd[] = {
  {"blit", cmd_blit},
  {0, 0}
};

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
  log_printf(LOG_DBG, "surface_size_fid_rm buf: '%p'\n", fid->aux);
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
}

static void
size_read(struct p9_connection *c)
{
  struct p9_fid *fid = c->t.pfid;
  read_buf_fn(c, strlen((char *)fid->aux), (char *)fid->aux);
}

static void
size_write(struct p9_connection *c)
{
  struct p9_fid *fid = c->t.pfid;
  write_buf_fn(c, size_buf_len - 1, (char *)fid->aux);
}

static void
size_clunk(struct p9_connection *c)
{
  struct surface *s = get_surface(c);
  struct p9_fid *fid = c->t.pfid;
  unsigned int w, h;

  if (!(fid->aux && s->img))
    return;

  if (sscanf((char *)fid->aux, "%u %u", &w, &h) != 2) {
    P9_SET_STR(c->r.ename, "Wrong image file format");
    return;
  }
  if (resize_surface(s, w, h)) {
    P9_SET_STR(c->r.ename, "Cannot resize blit surface");
    return;
  }
}

static void
pixels_open(struct p9_connection *c)
{
  struct surface *s = get_surface(c);

  if (!s->img) {
    P9_SET_STR(c->r.ename, "no image");
    return;
  }
}

static void
pixels_read(struct p9_connection *c)
{
  struct surface *s = get_surface(c);
  char *pix;

  if (!s->img)
    return;
  imlib_context_set_image(s->img);
  pix = (char *)imlib_image_get_data_for_reading_only();
  read_buf_fn(c, s->w * s->h * 4, pix);
}

static void
pixels_write(struct p9_connection *c)
{
  struct surface *s = get_surface(c);
  char *pix;

  if (!s->img)
    return;
  imlib_context_set_image(s->img);
  pix = (char *)imlib_image_get_data();
  write_buf_fn(c, s->w * s->h * 4, pix);
  imlib_image_put_back_data((DATA32 *)pix);
}

static void
pixels_clunk(struct p9_connection *c)
{
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

void
rm_surface(struct file *f)
{
  struct surface *s = (struct surface *)f->aux.p;
  if (s->img) {
    imlib_context_set_image(s->img);
    imlib_free_image();
  }
  s->img = 0;
}

int
init_surface(struct surface *s, int w, int h)
{
  int pixelsize = 4;

  memset(s, 0, sizeof(*s));
  s->w = w;
  s->h = h;

  s->img = imlib_create_image(w, h);
  if (!s->img)
    die("Cannot allocate memory");

  s->fs.mode = 0500 | P9_DMDIR;
  s->fs.qpath = ++qid_cnt;
  s->fs.aux.p = s;
  s->fs.rm = rm_surface;

  s->fs_ctl.file.name = "ctl";
  s->fs_ctl.file.mode = 0200 | P9_DMAPPEND;
  s->fs_ctl.file.qpath = ++qid_cnt;
  s->fs_ctl.file.fs = &ctl_fs;
  s->fs_ctl.file.aux.p = s;
  s->fs_ctl.cmd = ctl_cmd;
  add_file(&s->fs, &s->fs_ctl.file);

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

static void
cmd_blit(struct file *f, char *cmd)
{
  log_printf(LOG_DBG, "#surface/ctl blit\n");
}

int
resize_surface(struct surface *s, int w, int h)
{
  Imlib_Image newimg;

  if (w == s->w && h == s->h)
    return 0;
  imlib_context_set_image(s->img);
  imlib_context_set_anti_alias(0);
  newimg = imlib_create_cropped_image(0, 0, w, h);
  if (!newimg)
    return -1;
  imlib_free_image();
  s->img = newimg;
  s->w = w;
  s->h = h;
  return 0;
}
