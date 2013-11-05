#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "util.h"
#include "api.h"
#include "9p.h"
#include "fs.h"
#include "fstypes.h"
#include "backend.h"
#include "prop.h"
#include "uiobj.h"

struct uiobj_scroll {
  struct uiobj_container c;
  struct uiplace place;
  struct prop_intarr pos_fs;
  struct prop_intarr expand_fs;
  int pos[2];
  int expand[2];
  int scrollopts[2];
  int prevclip[4];
  struct uiobj *prevobj;
  enum {
    NORMAL = 0,
    WAITING,
    SCROLLING
  } mode;
  int prev_pos[2];
  int prev_ptr[2];
  unsigned int stop_ms;
};

static void
draw(struct uiobj *u, struct uicontext *ctx)
{
  int i, *cr, *sr, r[4];
  struct uiobj_scroll *us = (struct uiobj_scroll *)u->data;
  struct uiobj *child;

  if (!(us && us->place.obj))
    return;

  child = us->place.obj;
  for (sr = u->viewport.r, cr = child->viewport.r, i = 0; i < 4; ++i)
    cr[i] = sr[i];

  if (child->reqsize[0] < u->viewport.r[2]) {
    r[0] = child->reqsize[0];
    r[1] = u->viewport.r[1];
    r[2] = u->viewport.r[2] - child->reqsize[0];
    r[3] = u->viewport.r[3];
    draw_rect(screen_image, r[0], r[1], r[2], r[3], 0, SCROLL_BG);
  }
  if (child->reqsize[1] < u->viewport.r[3]) {
    r[0] = u->viewport.r[0];
    r[1] = child->reqsize[1];
    r[2] = u->viewport.r[2];
    r[3] = u->viewport.r[3] - child->reqsize[1];
    draw_rect(screen_image, r[0], r[1], r[2], r[3], 0, SCROLL_BG);
  }
}

static void
scroll_rect(int *r, struct uiobj *u, int coord)
{
  struct uiobj_scroll *us = (struct uiobj_scroll *)u->data;
  struct uiobj *child = us->place.obj;
  int pos, len, reqsize = child->reqsize[coord], t;

  t = u->viewport.r[coord + 2] - SCROLL_SIZE;
  pos = us->pos[coord] * t / reqsize;
  len = child->viewport.r[coord + 2] * t / reqsize;
  if (len < MIN_SCROLL_LEN)
    len = MIN_SCROLL_LEN;

  if (coord == 0) {
    r[0] = u->g.r[0] + pos;
    r[1] = u->g.r[1] + u->g.r[3] - SCROLL_SIZE;
    r[2] = len;
    r[3] = SCROLL_SIZE;
  } else {
    r[0] = u->g.r[0] + u->g.r[2] - SCROLL_SIZE;
    r[1] = u->g.r[1] + pos;
    r[2] = SCROLL_SIZE;
    r[3] = len;
  }
}

static void
draw_over(struct uiobj *u, struct uicontext *ctx)
{
  int i, r[4];
  struct uiobj_scroll *us = (struct uiobj_scroll *)u->data;
  struct uiobj *child;

  if (!(us && us->place.obj))
    return;

  child = us->place.obj;

  for (i = 0; i < 2; ++i)
    if (us->scrollopts[i] && child->reqsize[i] > u->viewport.r[i + 2]) {
      scroll_rect(r, u, i);
      draw_rect(screen_image, r[0], r[1], r[2], r[3], SCROLL_FRAME,
                SCROLL_HANDLE);
    }
}

static void
update_size(struct uiobj *u)
{
  struct uiobj_scroll *us = (struct uiobj_scroll *)u->data;
  struct uiobj *child;
  int i;

  u->reqsize[0] = u->reqsize[1] = 0;

  if (!(us && us->place.obj))
    return;
  child = us->place.obj;

  for (i = 0; i < 2; ++i)
    if (us->expand[i])
      u->reqsize[i] = child->reqsize[i];
}

static void
resize(struct uiobj *u)
{
  int i, r[4];
  struct uiobj_scroll *us = (struct uiobj_scroll *)u->data;
  struct uiobj *child;

  if (!(us && us->place.obj))
    return;

  child = us->place.obj;
  if (us->prevobj != child) {
    us->pos[0] = us->pos[1] = us->prev_pos[0] = us->prev_pos[1] = 0;
    u->flags |= UI_DIRTY_VISUAL;
  }
  for (i = 0; i < 2; ++i) {
    r[i] = u->g.r[i];
    r[i + 2] = (us->expand[i]) ? u->g.r[i + 2] : child->reqsize[i];
    us->scrollopts[i] = 0;
    if (child->reqsize[i] > u->g.r[2 + i]) {
      r[i] -= us->pos[i];
      us->scrollopts[i] = 1;
    } else
      us->pos[i] = 0;
  }
  ui_place_with_padding(&us->place, r);
  us->prevobj = child;
}

static struct file *
get_children(struct uiobj *u)
{
  struct uiobj_container *c = (struct uiobj_container *)u->data;
  return (c) ? c->f_items.child : 0;
}

static void
scroll(struct uiobj *u, int x, int y)
{
  struct uiobj_scroll *us = (struct uiobj_scroll *)u->data;
  struct uiobj *child = us->place.obj;
  int i, m;

  us->pos[0] = (x > 0) ? x : 0;
  us->pos[1] = (y > 0) ? y : 0;
  if (!child)
    return;
  for (i = 0; i < 2; ++i) {
    m = u->viewport.r[i] + u->viewport.r[i + 2];
    if (child->reqsize[i] - us->pos[i] < m)
      us->pos[i] = child->reqsize[i] - m;
  }
  if (us->prev_pos[0] != us->pos[0] || us->prev_pos[1] != us->pos[1]) {
    u->flags |= UI_DIRTY;
    child->flags |= UI_DIRTY;
    ui_enqueue_update(u);
    us->prev_pos[0] = us->pos[0];
    us->prev_pos[1] = us->pos[1];
  }
}

static void
scroll_rel(struct uiobj *u, int dx, int dy)
{
  struct uiobj_scroll *us = (struct uiobj_scroll *)u->data;
  scroll(u, us->pos[0] - dx, us->pos[1] - dy);
}

static int
on_ptr_move(struct uiobj *u, struct input_event *ev)
{
  struct uiobj_scroll *us = (struct uiobj_scroll *)u->data;
  struct uiobj *child = us->place.obj;
  int dx, dy;

  dx = ev->dx * child->reqsize[0] / u->g.r[2];
  dy = ev->dy * child->reqsize[1] / u->g.r[3];
  if (dx == 0 && dy == 0)
    return 0;

  switch (us->mode) {
  case NORMAL:
    us->prev_ptr[0] = ev->x;
    us->prev_ptr[1] = ev->y;
    us->mode = WAITING;
    break;
  case WAITING:
    if (abs(ev->x - us->prev_ptr[0]) < SCROLL_THRESHOLD
        && abs(ev->y - us->prev_ptr[1]) < SCROLL_THRESHOLD)
      return 0;
    us->mode = SCROLLING;
    break;
  case SCROLLING:
    scroll_rel(u, ev->x - us->prev_ptr[0], ev->y - us->prev_ptr[1]);
    us->prev_ptr[0] = ev->x;
    us->prev_ptr[1] = ev->y;
    break;
  }
  return 1;
}

static int
on_ptr_intersect(struct uiobj *u, int inside)
{
  struct uiobj_scroll *us = (struct uiobj_scroll *)u->data;
  if (us && us->mode != NORMAL) {
    us->mode = NORMAL;
    u->flags |= UI_DIRTY_VISUAL;
    if (us->place.obj)
      us->place.obj->flags |= UI_DIRTY_VISUAL;
  }
  return 0;
}

static int
on_input(struct uiobj *u, struct input_event *ev)
{
  struct uiobj_scroll *us = (struct uiobj_scroll *)u->data;

  if (!(us && us->place.obj))
    return 0;

  switch (ev->type) {
  case IN_PTR_MOVE:
    return (ev->state != 0) ? on_ptr_move(u, ev) : 0;

  case IN_PTR_UP:
  case IN_PTR_DOWN:
    if (us->mode != NORMAL) {
      us->mode = NORMAL;
      u->flags |= UI_DIRTY_VISUAL;
      us->place.obj->flags |= UI_DIRTY_VISUAL;
    }
    return 0;

  default: return 0;
  }
}

static void
pos_upd(struct prop *p)
{
  struct uiobj *u = (struct uiobj *)p->aux;
  struct uiobj_scroll *us = (struct uiobj_scroll *)u->data;
  scroll(u, us->pos[0], us->pos[1]);
}

static struct uiobj_ops scroll_ops = {
  .draw = draw,
  .draw_over = draw_over,
  .update_size = update_size,
  .resize = resize,
  .get_children = get_children,
  .on_input = on_input,
  .on_ptr_intersect = on_ptr_intersect,
};

int
init_uiscroll(struct uiobj *u)
{
  struct uiobj_scroll *x;

  u->data = 0;
  x = (struct uiobj_scroll *)calloc(1, sizeof(struct uiobj_scroll));
  if (!x)
    return -1;
  ui_init_container_items(&x->c, "items");
  x->c.f_items.fs = 0;
  x->c.f_items.mode = 0500 | P9_DMDIR;

  if (init_prop_intarr(&u->f, &x->pos_fs, "scrollpos", 2, x->pos, u)
      || init_prop_intarr(&u->f, &x->expand_fs, "expand", 2, x->expand, u)
      || ui_init_place(&x->place, 0)
      || arr_memcpy(&x->place.sticky.buf, 5, 0, 5, "tblr") < 0) {
    free(x);
    return -1;
  }
  x->pos_fs.p.update = x->expand_fs.p.update = ui_prop_update_default;
  x->pos_fs.p.update = pos_upd;
  x->place.f.name = "0";
  x->place.padding.r[0] = x->place.padding.r[1] = x->place.padding.r[2]
      = x->place.padding.r[3] = 0;
  add_file(&x->c.f_items, &x->place.f);
  u->ops = &scroll_ops;
  u->data = x;
  x->c.u = u;
  add_file(&u->f, &x->c.f_items);
  return 0;
}
