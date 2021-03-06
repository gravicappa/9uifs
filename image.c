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
#include "frontend.h"
#include "raster.h"
#include "image.h"
#include "stb_image.h"
#include "font.h"

const int size_buf_len = 32;

static int nimages = 0;
static struct image **images = 0;

static void cmd_blit(char *cmd, void *aux);
static void cmd_rect(char *cmd, void *aux);
static void cmd_line(char *cmd, void *aux);
static void cmd_poly(char *cmd, void *aux);
static void cmd_text(char *cmd, void *aux);

static struct ctl_cmd ctl_cmd[] = {
  {"blit", cmd_blit},
  {"rect", cmd_rect},
  {"line", cmd_line},
  {"poly", cmd_poly},
  {"text", cmd_text},
  {0}
};

static struct image *
get_image(struct p9_connection *con)
{
  struct p9_fid *fid = con->t.pfid;
  return (struct image *)((struct file *)fid->file)->parent;
}

static void
update_rect(struct image *s, int *r)
{
  struct image_link *link;
  for (link = s->links; link; link = link->next)
    if (link->update)
      link->update(link->ptr, r);
}

static int
resize(struct image *s, int w, int h)
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
id_open(struct p9_connection *con)
{
  struct p9_fid *fid = con->t.pfid;
  struct image *img = (struct image *)((struct file *)fid->file)->parent;

  fid->aux = calloc(1, size_buf_len);
  if (!fid->aux) {
    P9_SET_STR(con->r.ename, "out of memory");
    return;
  }
  snprintf((char *)fid->aux, size_buf_len, "%u", img->id);
  fid->rm = rm_fid_aux;
}

static void
str_read(struct p9_connection *con)
{
  char *s = (char *)con->t.pfid->aux;
  read_data_fn(con, strlen(s), s);
}

static void
size_open(struct p9_connection *con)
{
  struct p9_fid *fid = con->t.pfid;
  struct image *s = (struct image *)((struct file *)fid->file)->parent;

  fid->aux = calloc(1, size_buf_len);
  if (!fid->aux) {
    P9_SET_STR(con->r.ename, "out of memory");
    return;
  }
  if (!(con->t.mode & P9_OTRUNC) || P9_READ_MODE(con->t.mode))
    snprintf((char *)fid->aux, size_buf_len, "%u %u", s->w, s->h);
  fid->rm = rm_fid_aux;
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
  struct image *s = (struct image *)((struct file *)fid->file)->parent;
  unsigned int w, h;

  if (!(fid->aux && s->img && P9_WRITE_MODE(fid->open_mode)))
    return;

  if (sscanf((char *)fid->aux, "%u %u", &w, &h) != 2) {
    P9_SET_STR(con->r.ename, "Wrong image file format");
    return;
  }
  if (s->w != w || s->h != h) {
    if (resize(s, w, h)) {
      P9_SET_STR(con->r.ename, "Cannot resize blit image");
      return;
    }
    update_rect(s, 0);
  }
}

static void
pixels_open(struct p9_connection *con)
{
  struct p9_fid *fid = con->t.pfid;
  struct image *s = (struct image *)((struct file *)fid->file)->parent;

  if (!s->img) {
    P9_SET_STR(con->r.ename, "no image");
    return;
  }
  s->flags &= ~IMAGE_DIRTY;
}

static void
pixels_read(struct p9_connection *con)
{
  struct image *s = get_image(con);
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
  struct image *s = get_image(con);
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
    s->flags |= IMAGE_DIRTY;
}

static void
pixels_clunk(struct p9_connection *con)
{
  struct image *s = get_image(con);
  /* TODO: calculate smallest rectangle */
  if (s->flags & IMAGE_DIRTY)
    update_rect(s, 0);
  s->flags &= ~IMAGE_DIRTY;
}

static void
png_open(struct p9_connection *con)
{
  struct p9_fid *fid = con->t.pfid;
  fid->aux = 0;
  fid->rm = rm_fid_aux;
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
  struct image *s = get_image(con);
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
  update_rect(s, 0);
}

static struct p9_fs image_id_fs = {
  .open = id_open,
  .read = str_read
};

static struct p9_fs image_size_fs = {
  .open = size_open,
  .read = str_read,
  .write = size_write,
  .clunk = size_clunk,
  .remove = size_clunk
};

static struct p9_fs image_pixels_fs = {
  .open = pixels_open,
  .read = pixels_read,
  .write = pixels_write,
  .clunk = pixels_clunk,
};

static struct p9_fs image_png_fs = {
  .open = png_open,
  .write = png_write,
  .clunk = png_clunk
};

static void
rm_image_data(struct file *f)
{
  struct image *s = (struct image *)f;
  struct image_link *link, *link_next;
  for (link = s->links; link; link = link_next) {
    link_next = link->next;
    if (link->rm)
      link->rm(link->ptr);
    free(link);
  }
  if (s->img) {
    free_image(s->img);
    s->img = 0;
  }
}

static void
rm_image(struct file *f)
{
  struct image *img = (struct image *)f;
  if (!img)
    return;
  images[img->id] = 0;
  rm_image_data(f);
  free(f);
}

int
init_image(struct image *s, int w, int h, struct client *c)
{
  memset(s, 0, sizeof(*s));
  s->w = w;
  s->h = h;
  s->client = c;

  s->img = create_image(w, h, 0);
  if (!s->img && w && h)
    return -1;

  s->f.mode = 0500 | P9_DMDIR;
  s->f.qpath = new_qid(FS_IMAGE);
  s->f.rm = rm_image_data;

  s->ctl = mk_ctl("ctl", ctl_cmd, &s->f);
  if (!s->ctl) {
    free_image(s->img);
    return -1;
  }
  add_file(&s->f, s->ctl);

  s->f_id.name = "id";
  s->f_id.mode = 0400;
  s->f_id.qpath = new_qid(0);
  s->f_id.fs = &image_id_fs;
  add_file(&s->f, &s->f_id);

  s->f_size.name = "size";
  s->f_size.mode = 0600;
  s->f_size.qpath = new_qid(0);
  s->f_size.fs = &image_size_fs;
  add_file(&s->f, &s->f_size);

  s->f_pixels.name = "rgba";
  s->f_pixels.mode = 0600;
  s->f_pixels.qpath = new_qid(0);
  s->f_pixels.fs = &image_pixels_fs;
  s->f_pixels.length = w * h * 4;
  add_file(&s->f, &s->f_pixels);

  s->f_in_png.name = "in.png";
  s->f_in_png.mode = 0200;
  s->f_in_png.qpath = new_qid(0);
  s->f_in_png.fs = &image_png_fs;
  add_file(&s->f, &s->f_in_png);
  return 0;
}

struct image *
mk_image(int w, int h, struct client *c)
{
  struct image *img;
  int i, delta = 8;
  for (i = 0; i < nimages && images[i]; ++i) {}
  if (i >= nimages) {
    images = realloc(images, (nimages + delta) * sizeof(struct image *));
    if (!images)
      return 0;
    memset(images + nimages, 0, delta * sizeof(struct image *));
    nimages += delta;
  }
  img = malloc(sizeof(struct image));
  if (img && init_image(img, w, h, c)) {
    free(img);
    return 0;
  }
  img->f.rm = rm_image;
  images[i] = img;
  img->id = i;
  return img;
}

static void
cmd_blit(char *cmd, void *aux)
{
  int sx = 0, sy = 0, sw, sh, r[4] = {0}, id, n;
  static const char *fmt = "%d %d %d %d %d %d %d %d %d";
  struct image *src, *dst = aux;

  n = sscanf(cmd, fmt, &id, &r[0], &r[1], &r[2], &r[3], &sx, &sy, &sw, &sh);
  src = (id >= 0 && id < nimages) ? images[id] : 0;
  if (!src)
    return;
  switch (n) {
    case 3: r[2] = src->w;
    case 4: r[3] = src->h;
    case 5: sx = 0;
    case 6: sy = 0;
    case 7: sw = src->w;
    case 8: sh = src->h;
    case 9: break;
    default: return;
  }
  blit_image(dst->img, r[0], r[1], r[2], r[3], src->img, sx, sy, sw, sh);
  update_rect(dst, r);
}

static int
colour_from_str(const char *s)
{
  switch (strlen(s)) {
  case 1: return RGBA_FROM_STR1(s);
  case 2: return RGBA_FROM_STR2(s);
  case 3: return RGBA_FROM_STR3(s);
  case 4: return RGBA_FROM_STR4(s);
  case 6: return RGBA_FROM_STR6(s);
  case 8: return RGBA_FROM_STR8(s);
  default: return 0;
  }
}

static void
cmd_rect(char *cmd, void *aux)
{
  int r[4], i, bg = 0, fg = 0xff000000;
  char *arg, *c = cmd;
  struct image *s = aux;

  if (!(arg = next_arg(&c)))
    return;
  fg = colour_from_str(arg);

  if (!(arg = next_arg(&c)))
    return;
  bg = colour_from_str(arg);

  while (c) {
    for (i = 0; i < 4; ++i)
      if (!(arg = next_arg(&c)) || sscanf(arg, "%d", &r[i]) != 1)
        break;
    if (i < 4)
      break;
    draw_rect(s->img, r[0], r[1], r[2], r[3], fg, bg);
  }
  update_rect(s, r);
}

static void
cmd_line(char *cmd, void *aux)
{
  int r[4], i, fg = 0xff000000;
  char *arg, *c = cmd;
  struct image *s = aux;
  if (!(arg = next_arg(&c)))
    return;
  fg = colour_from_str(arg);
  if (!RGBA_VISIBLE(fg))
    return;
  while (c) {
    for (i = 0; i < 4; ++i)
      if (!(arg = next_arg(&c)) || sscanf(arg, "%d", &r[i]) != 1)
        break;
    if (i < 4)
      break;
    draw_line(s->img, r[0], r[1], r[2], r[3], fg);
  }
  update_rect(s, r);
}

static void
draw_linestrip(struct image *s, int fg, char *args)
{
  char *arg;
  int i, pt[2], prevpt[2], r[4] = {0};
  UImage img = s->img;
  for (i = 0; i < 2; ++i)
    if (!(arg = next_arg(&args)) || sscanf(arg, "%d", &prevpt[i]) != 1)
      return;
  r[0] = prevpt[0];
  r[1] = prevpt[1];
  while (args) {
    for (i = 0; i < 2; ++i)
      if (!(arg = next_arg(&args)) || sscanf(arg, "%d", &pt[i]) != 1)
        break;
    if (i < 2)
      break;
    draw_line(img, prevpt[0], prevpt[1], pt[0], pt[1], fg);
    for (i = 0; i < 2; ++i) {
      prevpt[i] = pt[i];
      if (r[i] + r[i + 2] < pt[i])
        r[i + 2] = pt[i] - r[i];
      else if (r[i] > pt[i])
        r[i] = pt[i];
    }
  }
  if (r[2] || r[3])
    update_rect(s, r);
}

#define STATIC_NPTS 32

static void
cmd_poly(char *cmd, void *aux)
{
  struct image *s = aux;
  char *arg, *c = cmd;
  int spts[STATIC_NPTS * 2];
  int i, n, npts, *pts = spts, *p, bg = 0, fg = 0xff000000, r[4] = {0};

  if (!(arg = next_arg(&c)))
    return;
  fg = colour_from_str(arg);
  if (!(arg = next_arg(&c)))
    return;
  bg = colour_from_str(arg);
  npts = nargs(c) >> 1;
  if (!npts)
    return;
  if (!RGBA_VISIBLE(bg)) {
    draw_linestrip(s, fg, c);
    return;
  }
  if (npts > STATIC_NPTS) {
    pts = malloc(sizeof(int) * npts * 2);
    if (!pts) {
      /* out of memory. shoot out client */
      return;
    }
  }
  for (n = 0, p = pts, i = npts * 2; i; --i, ++p)
    if ((arg = next_arg(&c)) && sscanf(arg, "%d", p) == 1) {
      if (r[n & 1] + r[(n & 1) + 1] < *p)
        r[(n & 1) + 1] = *p - r[n & 1];
      else if (r[n & 1] > *p)
        r[n & 1] = *p;
      ++n;
    }
  if (n == npts * 2) {
    draw_poly(s->img, npts, pts, fg, bg);
    update_rect(s, r);
  }
  if (pts != spts)
    free(pts);
}

static void
cmd_text(char *cmd, void *aux)
{
  struct image *s = aux;
  char *arg;
  UFont font;
  int fg = 0, pt[2], i, r[4], n;

  if (!(arg = next_arg(&cmd)))
    return;
  font = font_from_str(arg);
  if (!(arg = next_arg(&cmd))) {
    free_font(font);
    return;
  }
  fg = colour_from_str(arg);
  for (i = 0; i < 2; ++i)
    if (!(arg = next_arg(&cmd)) || sscanf(arg, "%d", &pt[i]) != 1)
      return;
  r[0] = pt[0];
  r[1] = pt[1];
  n = strlen(cmd);
  get_utf8_size(font, n, cmd, &r[2], &r[3]);
  draw_utf8(s->img, pt[0], pt[1], fg, font, n, cmd);
  update_rect(s, r);
  free_font(font);
}

struct image_link *
link_image(struct image *s, void *ptr)
{
  struct image_link *link;

  if (!s)
    return 0;
  unlink_image(s, ptr);
  link = calloc(1, sizeof(struct image_link));
  if (link) {
    link->next = s->links;
    s->links = link;
    link->ptr = ptr;
  }
  return link;
}

void
unlink_image(struct image *s, void *ptr)
{
  struct image_link **pp, *link;
  if (!s)
    return;
  pp = &s->links;
  for (link = *pp; link && link->ptr != ptr; pp = &link->next, link = *pp) {}
  if (link) {
    *pp = link->next;
    if (link->rm)
      link->rm(ptr);
    free(link);
  }
}
