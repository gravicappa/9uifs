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
#include "prop.h"
#include "ui.h"

extern int init_uigrid(struct uiobj *);

struct uiobj_maker {
  char *type;
  int (*init)(struct uiobj *);
} uitypes[] = {
  {"grid", init_uigrid},
  /*
  {"button", mk_uibutton},
  {"label", mk_uilabel},
  {"entry", mk_uientry},
  {"blit", mk_uientry},
  */
  {0, 0}
};

static void
prop_type_open(struct p9_connection *c)
{
  struct prop *p = (struct prop *)c->t.pfid->file;
  struct uiobj *u = (struct uiobj *)p->aux;

  if (u->type.buf && u->type.buf->used && u->type.buf->b[0]
      && (((c->t.pfid->open_mode & 3) == P9_OWRITE)
          || ((c->t.pfid->open_mode & 3) == P9_ORDWR))) {
    P9_SET_STR(c->r.ename, "UI object already has type");
    return;
  }
}

static void
prop_type_clunk(struct p9_connection *c)
{
  struct prop_buf *p = (struct prop_buf *)c->t.pfid->file;
  int i, good = 0;

  if (((c->t.pfid->open_mode & 3) == P9_OWRITE)
      || ((c->t.pfid->open_mode & 3) == P9_ORDWR)) {
    if (!p->buf)
      return;
    trim_string_right(p->buf->b, " \n\r\t");
    for (i = 0; uitypes[i].type && uitypes[i].init; ++i)
      if (!strcmp(uitypes[i].type, p->buf->b))
        if (uitypes[i].init((struct uiobj *)p->p.aux) == 0)
          good = 1;
    if (!good)
      memset(p->buf->b, 0, p->buf->size);
  }
}

struct p9_fs prop_type_fs = {
  .open = prop_type_open,
  .read = prop_buf_read,
  .write = prop_buf_write,
  .clunk = prop_type_clunk
};

static void
draw_uiobj(struct uiobj *u)
{
  unsigned int c;

  c = u->bg.i;
  if (c && 0xff000000)
    fill_rect(u->v->blit.img, u->g.x, u->g.y, u->g.w, u->g.h, c);
}

void
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
  r = init_prop_buf(&u->fs, &u->type, "type", 0, "", 0, u)
      | init_prop_colour(&u->fs, &u->bg, "background", 0, u)
      | init_prop_int(&u->fs, &u->visible, "visible", 0, u)
      | init_prop_int(&u->fs, &u->minwidth, "minwidth", 0, u)
      | init_prop_int(&u->fs, &u->maxwidth, "maxwidth", 0, u)
      | init_prop_int(&u->fs, &u->minheight, "minheight", 0, u)
      | init_prop_int(&u->fs, &u->maxheight, "maxheight", 0, u);

  if (r) {
    rm_file(&u->fs);
    free(u);
    u = 0;
  }

  u->type.p.fs.fs = &prop_type_fs;

  u->fs_evfilter.name = "evfilter";
  u->fs_evfilter.mode = 0600;
  u->fs_evfilter.qpath = ++qid_cnt;
  u->fs_evfilter.aux.p = u;
  /*u->fs_evfilter.fs = &evfilter_fs;*/
  add_file(&u->fs, &u->fs_evfilter);

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
