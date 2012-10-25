#include <stdlib.h>
#include <stddef.h>
#include "util.h"
#include "draw.h"
#include "9p.h"
#include "fs.h"
#include "fsutil.h"
#include "prop.h"
#include "event.h"
#include "ctl.h"
#include "surface.h"
#include "view.h"
#include "uiobj.h"
#include "client.h"
#include "config.h"

struct uiobj_image {
  struct surface *s;
  struct file f_path;
  struct uiobj *obj;
};

static void
rm_fid_aux(struct p9_fid *fid)
{
  if (fid->aux)
    free(fid->aux);
  fid->rm = 0;
}

static void
path_open(struct p9_connection *con)
{
  struct uiobj_image *img;
  struct p9_fid *fid = con->t.pfid;
  int n;
  struct arr *buf = 0;

  img = containerof(fid->file, struct uiobj_image, f_path);
  fid->aux = 0;
  fid->rm = rm_fid_aux;

  if (img->s && !(con->t.mode & P9_OTRUNC) && P9_READ_MODE(con->t.mode)) {
    n = file_path_len((struct file *)img->s, img->obj->client->images);
    if (arr_memcpy(&buf, n, 0, n, 0))
      die("Cannot allocate memory");
    file_path(n, buf->b, (struct file *)img->s, img->obj->client->images);
    fid->aux = buf;
  }
}

static void
path_write(struct p9_connection *con)
{
  write_buf_fn(con, 16, (struct arr **)&con->t.pfid->aux);
}

static void
path_read(struct p9_connection *con)
{
  struct arr *arr = con->t.pfid->aux;
  if (arr)
    read_str_fn(con, arr->used, arr->b);
}

static void
path_clunk(struct p9_connection *con)
{
  struct p9_fid *fid = con->t.pfid;
  struct uiobj_image *img;
  struct arr *buf;
  int prevw = 0, prevh = 0;
  struct surface *prevs;
  struct client *client;

  if (!P9_WRITE_MODE(fid->open_mode))
    return;

  img = containerof(fid->file, struct uiobj_image, f_path);
  client = img->obj->client;
  buf = fid->aux;
  if (client && buf && buf->used > 0) {
    prevs = img->s;
    if (prevs) {
      prevw = prevs->w;
      prevh = prevs->h;
    }
    for (; buf->b[buf->used - 1] <= ' '; --buf->used) {}
    img->s = (struct surface *)find_file(client->images, buf->used, buf->b);
  }
  if (img->s != prevs) {
    if (!img->s || img->s->w != prevw || img->s->h != prevh)
      ui_propagate_dirty(img->obj->parent);
    img->obj->flags |= UI_DIRTY;
  }
}

static struct p9_fs path_fs = {
  .open = path_open,
  .read = path_read,
  .write = path_write,
  .clunk = path_clunk
};

static void
update_size(struct uiobj *u)
{
  struct uiobj_image *img = u->data;
  if (!(img && img->s))
    return;
  u->reqsize[0] = img->s->w;
  u->reqsize[1] = img->s->h;
}

static void
draw(struct uiobj *u, struct uicontext *ctx)
{
  struct uiobj_image *img = u->data;
  struct surface *blit = &ctx->v->blit;
  struct surface *s = img->s;
  int *r = u->g.r;

  if (s && s->img)
    blit_image(blit->img, r[0], r[1], r[2], r[3], s->img, 0, 0, s->w, s->h);
}

static struct uiobj_ops image_ops = {
  .draw = draw,
  .update_size = update_size
};

int
init_uiimage(struct uiobj *u)
{
  struct uiobj_image *img;

  img = calloc(1, sizeof(struct uiobj_image));
  if (!img)
    return -1;
  img->f_path.name = "path";
  img->f_path.mode = 0600;
  img->f_path.qpath = new_qid(0);
  img->f_path.fs = &path_fs;
  img->obj = u;
  u->data = img;
  u->ops = &image_ops;
  add_file(&u->f, &img->f_path);
  return 0;
}
