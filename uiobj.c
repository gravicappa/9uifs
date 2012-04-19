#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <Imlib2.h>

#include "util.h"
#include "9p.h"
#include "fs.h"
#include "fsutil.h"
#include "geom.h"
#include "event.h"
#include "ctl.h"
#include "surface.h"
#include "draw.h"
#include "view.h"
#include "ui.h"

#define INT_BUF_SIZE 16

static void
int_free(struct p9_fid *fid)
{
  if (fid->aux)
    free(fid->aux);
}

static void
int_open(struct p9_connection *c, int size, const char *fmt)
{
  struct uiprop_int *p;
  struct p9_fid *fid = c->t.pfid;

  p = (struct uiprop_int *)fid->file;
  fid->aux = malloc(size);
  memset(fid->aux, 0, size);
  fid->rm = int_free;
  if (!(c->t.mode & P9_OTRUNC))
    snprintf((char *)fid->aux, size, fmt, p->i);
}

static int
int_clunk(struct p9_connection *c, const char *fmt)
{
  int x;
  struct uiprop_int *p;

  if (!c->t.pfid->aux)
    return 0;

  p = (struct uiprop_int *)c->t.pfid->file;
  if (sscanf((char *)c->t.pfid->aux, fmt, &x) != 1)
    return -1;
  if (p) {
    p->i = x;
    if (p->p.update)
      p->p.update(&p->p);
  }
  return 0;
}


static void
int10_open(struct p9_connection *c)
{
  int_open(c, INT_BUF_SIZE, "%d");
}

static void
int10_read(struct p9_connection *c)
{
  read_buf_fn(c, strlen((char *)c->t.pfid->aux), (char *)c->t.pfid->aux);
}

static void
int10_write(struct p9_connection *c)
{
  write_buf_fn(c, INT_BUF_SIZE - 1, (char *)c->t.pfid->aux);
}

static void
int10_clunk(struct p9_connection *c)
{
  if (int_clunk(c, "%d"))
    P9_SET_STR(c->r.ename, "Wrong number format");
}

static void
buf_open(struct p9_connection *c)
{
  struct uiprop_buf *p = (struct uiprop_buf *)c->t.pfid->file;
  if (c->t.mode & P9_OTRUNC && p->buf) {
    p->buf->used = 0;
    memset(p->buf->b, 0, p->buf->size);
  }
}

static void
buf_read(struct p9_connection *c)
{
  struct uiprop_buf *p = (struct uiprop_buf *)c->t.pfid->file;
  if (p->buf)
    read_buf_fn(c, strlen(p->buf->b), p->buf->b);
}

static void
buf_write(struct p9_connection *c)
{
  struct uiprop_buf *p = (struct uiprop_buf *)c->t.pfid->file;
  if (p->buf)
    write_buf_fn(c, p->buf->size - 1, p->buf->b);
}

static void
str_write(struct p9_connection *c)
{
  struct uiprop_buf *p = (struct uiprop_buf *)c->t.pfid->file;
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

static void
prop_clunk(struct p9_connection *c)
{
  struct uiprop *p = (struct uiprop *)c->t.pfid->file;

  if (p && p->update)
    p->update(p);
}

static void
colour_open(struct p9_connection *c)
{
  int_open(c, 8, "%08x");
}

static void
colour_read(struct p9_connection *c)
{
  read_buf_fn(c, 8, (char *)c->t.pfid->aux);
}

static void
colour_write(struct p9_connection *c)
{
  write_buf_fn(c, 8, (char *)c->t.pfid->aux);
}

static void
colour_clunk(struct p9_connection *c)
{
  if (int_clunk(c, "%08x"))
    P9_SET_STR(c->r.ename, "Wrong colour format");
}

struct p9_fs int_fs = {
  .open = int10_open,
  .read = int10_read,
  .write = int10_write,
  .clunk = int10_clunk
};

struct p9_fs buf_fs = {
  .open = buf_open,
  .read = buf_read,
  .write = buf_write,
  .clunk = prop_clunk
};

struct p9_fs str_fs = {
  .open = buf_open,
  .read = buf_read,
  .write = str_write,
  .clunk = prop_clunk
};

struct p9_fs colour_fs = {
  .open = colour_open,
  .read = colour_read,
  .write = colour_write,
  .clunk = colour_clunk
};

static void
buf_prop_rm(struct file *f)
{
  struct uiprop_buf *p;
  p = (struct uiprop_buf *)f->aux.p;
  if (p && p->buf) {
    free(p->buf);
    p->buf = 0;
  }
}

static void
init_prop_fs(struct uiobj *u, struct uiprop *p, char *name)
{
  p->obj = u;
  p->fs.name = name;
  p->fs.mode = 0600;
  p->fs.qpath = ++qid_cnt;
  p->fs.aux.p = p;
}

int
ui_init_prop_int(struct uiobj *u, struct uiprop_int *p, char *name, int x)
{
  memset(p, 0, sizeof(*p));
  init_prop_fs(u, &p->p, name);
  p->i = x;
  p->p.fs.fs = &int_fs;
  add_file(&u->fs, &p->p.fs);
  return 0;
}

static int
init_prop_buf(struct uiobj *u, struct uiprop_buf *p, char *name,
              int size, char *x)
{
  init_prop_fs(u, &p->p, name);
  if (arr_memcpy(&p->buf, 32, -1, size + 1, 0) < 0)
    return -1;
  if (x) {
    memcpy(p->buf->b, x, size);
    p->buf->b[size] = 0;
  } else
    memset(p->buf->b, 0, p->buf->size);
  p->p.fs.fs = &buf_fs;
  p->p.fs.rm = buf_prop_rm;
  add_file(&u->fs, &p->p.fs);
  return 0;
}

int
ui_init_prop_buf(struct uiobj *u, struct uiprop_buf *p, char *name, int size,
                 char *x)
{
  if (init_prop_buf(u, p, name, size, x))
    return -1;
  p->p.fs.fs = &buf_fs;
  return 0;
}

int
ui_init_prop_str(struct uiobj *u, struct uiprop_buf *p, char *name, int size,
                 char *x)
{
  if (init_prop_buf(u, p, name, size, x))
    return -1;
  p->p.fs.fs = &str_fs;
  return 0;
}

int
ui_init_prop_colour(struct uiobj *u, struct uiprop_int *p, char *name,
                    unsigned int rgba)
{
  memset(p, 0, sizeof(*p));
  init_prop_fs(u, &p->p, name);
  p->i = rgba;
  p->p.fs.fs = &colour_fs;
  add_file(&u->fs, &p->p.fs);
  return 0;
}

static void
draw_uiobj(struct uiobj *u)
{
  unsigned int c;

  c = u->bg.i;
  if (c && 0xff000000)
    fill_rect(u->v->blit.img, u->g.x, u->g.y, u->g.w, u->g.h, c);
}

static void
rm_uiobj(struct file *f)
{
  struct uiobj *u = (struct uiobj *)f->aux.p;
  if (!u)
    return;
  free(u);
}

struct uiobj *
mk_uiobj()
{
  int r;
  struct uiobj *u;

  u = (struct uiobj *)malloc(sizeof(struct uiobj));
  if (!u)
    return 0;
  memset(u, 0, sizeof(*u));
  u->draw = draw_uiobj;
  u->fs.mode = 0500 | P9_DMDIR;
  u->fs.qpath = ++qid_cnt;
  u->fs.rm = rm_uiobj;

  /* TODO: use bg from some kind of style db */
  r = ui_init_prop_colour(u, &u->bg, "background", 0)
      | ui_init_prop_int(u, &u->visible, "visible", 0)
      | ui_init_prop_int(u, &u->minwidth, "minwidth", 0)
      | ui_init_prop_int(u, &u->maxwidth, "maxwidth", 0)
      | ui_init_prop_int(u, &u->minheight, "minheight", 0)
      | ui_init_prop_int(u, &u->maxheight, "maxheight", 0);

  if (r) {
    rm_file(&u->fs);
    free(u);
    u = 0;
  }

  u->fs_evfilter.name = "evfilter";
  u->fs_evfilter.mode = 0600;
  u->fs_evfilter.qpath = ++qid_cnt;
  u->fs_evfilter.aux.p = u;
  /*u->fs_evfilter.fs = &evfilter_fs;*/
  add_file(&u->fs, &u->fs_evfilter);

  u->fs_type.name = "type";
  u->fs_type.mode = 0600;
  u->fs_type.qpath = ++qid_cnt;
  u->fs_type.aux.p = u;
  /*u->fs_type.fs = &type_fs;*/
  add_file(&u->fs, &u->fs_type);

  u->fs_g.name = "g";
  u->fs_g.mode = 0400;
  u->fs_g.qpath = ++qid_cnt;
  u->fs_g.aux.p = u;
  /*u->fs_g.fs = &geom_fs;*/
  add_file(&u->fs, &u->fs_g);

  return u;
}

static void
update_obj_size(struct uiobj *u)
{
  if (u->update_size)
    u->update_size(u);
  u->req_w = u->minwidth.i;
  u->req_h = u->minheight.i;
}

static struct file *
up_children(struct uiobj_place *up)
{
  if (!(up->obj && (up->obj->flags & UI_IS_CONTAINER) && up->obj->data))
    return 0;
  return ((struct uiobj_container *)up->obj->data)->fs_items.child;
}

void
ui_update_size(struct uiobj_place *up)
{
  struct file *f;
  struct uiobj_place *x, *t;

  if (!(up->obj && (up->obj->flags & UI_IS_CONTAINER)))
    return;
  up->parent = 0;
  x = up;
  do {
    f = up_children(x);
    t = x;
    if (f) {
      x = (struct uiobj_place *)f->aux.p;
      x->parent = t;
    } else if (x->fs.next) {
      update_obj_size(x->obj);
      x = (struct uiobj_place *)x->fs.next->aux.p;
      x->parent = t->parent;
    } else {
      while (x && x != up && x->parent && !x->parent->fs.next) {
        update_obj_size(x->obj);
        x = x->parent;
      }
      update_obj_size(x->obj);
      if (x->parent && x->parent != up)
        update_obj_size(x->parent->obj);
      if (x != up && x->parent && x->parent->fs.next
          && x->parent->fs.next != &up->obj->fs)
        x = x->parent->fs.next->aux.p;
    }
  } while (x && x != up);
}
