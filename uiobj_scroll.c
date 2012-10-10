#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "util.h"
#include "input.h"
#include "9p.h"
#include "fs.h"
#include "fstypes.h"
#include "draw.h"
#include "prop.h"

#include "ctl.h"
#include "event.h"
#include "surface.h"
#include "view.h"

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
};

static void
draw(struct uiobj *u, struct uicontext *ctx)
{
  int *clip = ctx->clip, i, *cr, *r;
  struct uiobj_scroll *us = (struct uiobj_scroll *)u->data;
  struct uiobj *child;

  if (!(us && us->place.obj))
    return;
  child = us->place.obj;
  r = u->viewport.r;
  cr = child->viewport.r;
  for (i = 0; i < 4; ++i)
    cr[i] = r[i];
  for (i = 0; i < 2; ++i)
    if (us->scrollopts[i] && child->reqsize[i])
      cr[i + 2] -= SCROLL_SIZE;
}

static void
scroll_rect(int *r, struct uiobj *u, int coord)
{
  if (coord == 0) {
    r[0] = u->g.r[0];
    r[1] = u->g.r[1] + u->g.r[3] - SCROLL_SIZE;
    r[2] = u->g.r[2];
    r[3] = SCROLL_SIZE;
  } else {
    r[0] = u->g.r[0] + u->g.r[2] - SCROLL_SIZE;
    r[1] = u->g.r[1];
    r[2] = SCROLL_SIZE;
    r[3] = u->g.r[3];
  }
}

static void
draw_over(struct uiobj *u, struct uicontext *ctx)
{
  int i, flag = 0, len[2], pos[2], r[4], *clip = ctx->clip;
  struct uiobj_scroll *us = (struct uiobj_scroll *)u->data;
  struct uiobj *child;
  struct surface *blit;

  if (!(us && us->place.obj))
    return;

  child = us->place.obj;

  for (i = 0; i < 2; ++i)
    if (us->scrollopts[i] && child->reqsize[i]) {
      flag |= 1 << i;
      len[i] = child->g.r[i + 2] * u->g.r[i + 2] / child->reqsize[i];
      if (len[i] < MIN_SCROLL_LEN)
        len[i] = MIN_SCROLL_LEN;
      pos[i] = us->pos[i] * u->g.r[i + 2] / child->reqsize[i];
    }

  blit = &ctx->v->blit;
  if (flag & 1) {
    scroll_rect(r, u, 0);
    fill_rect(blit->img, r[0], r[1], r[2], r[3], SCROLL_BG);
    r[0] += pos[0];
    r[2] = len[0];
    fill_rect(blit->img, r[0], r[1], r[2], r[3], SCROLL_FG);
  }
  if (flag & 2) {
    scroll_rect(r, u, 1);
    fill_rect(blit->img, r[0], r[1], r[2], r[3], SCROLL_BG);
    r[1] += pos[1];
    r[3] = len[1];
    fill_rect(blit->img, r[0], r[1], r[2], r[3], SCROLL_FG);
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
  if (us->prevobj != child)
    us->pos[0] = us->pos[1] = 0;

  for (i = 0; i < 2; ++i) {
    r[i] = u->g.r[i];
    r[i + 2] = child->reqsize[i];
    us->scrollopts[i] = 0;
    if (child->reqsize[i] > u->g.r[2 + i]) {
      r[i] -= us->pos[i];
      us->scrollopts[i] = 1;
    }
  }
  ui_place_with_padding(&us->place, r);
  log_printf(LOG_UI, "scroll.resize [%d %d %d %d]\n", child->g.r[0],
             child->g.r[1], child->g.r[2], child->g.r[3]);
  us->prevobj = child;
}

static struct file *
get_children(struct uiobj *u)
{
  struct uiobj_container *c = (struct uiobj_container *)u->data;
  return (c) ? c->fs_items.child : 0;
}

static int
on_input(struct uiobj *u, struct input_event *ev)
{
  struct uiobj_scroll *us = (struct uiobj_scroll *)u->data;
  struct uiobj *child;
  int i, dx, dy;

  if (!(us && us->place.obj))
    return 0;
  child = us->place.obj;

  log_printf(LOG_UI, "scroll-on-input ev-state: %d\n", ev->state);
  switch (ev->type) {
  case IN_PTR_MOVE:
    if (ev->state == 0)
      return 0;
    log_printf(LOG_UI, "scroll-on-input ev-delta: [%d %d]\n", ev->dx, ev->dy);
    dx = ev->dx * child->reqsize[0] / u->g.r[2];
    dy = ev->dy * child->reqsize[1] / u->g.r[3];
    if (dx == 0 && dy == 0)
      return 0;
    us->pos[0] -= dx;
    us->pos[1] -= dy;
    log_printf(LOG_UI, "scroll-on-input pos: [%d %d]\n", us->pos[0],
               us->pos[1]);
    u->flags |= UI_IS_DIRTY;
    child->flags |= UI_IS_DIRTY;
    resize(u);
    memcpy(child->viewport.r, u->g.r, sizeof(child->viewport.r));
    for (i = 0; i < 2; ++i)
      if (us->pos[i] < 0)
        us->pos[i] = 0;
      else if (us->pos[i] > child->reqsize[i])
        us->pos[i] = child->reqsize[i];
    log_printf(LOG_UI, "            new pos: [%d %d]\n", us->pos[0],
               us->pos[1]);
    return 1;
  default: return 0;
  }
}

static struct uiobj_ops scroll_ops = {
  .draw = draw,
  .draw_over = draw_over,
  .update_size = update_size,
  .resize = resize,
  .get_children = get_children,
  .on_input = on_input,
};

int
init_uiscroll(struct uiobj *u)
{
  struct uiobj_scroll *x;

  u->data = 0;
  x = (struct uiobj_scroll *)malloc(sizeof(struct uiobj_scroll));
  if (!x)
    return -1;
  memset(x, 0, sizeof(struct uiobj_scroll));
  ui_init_container_items(&x->c, "items");
  x->c.fs_items.fs = 0;
  x->c.fs_items.mode = 0500 | P9_DMDIR;

  if (init_prop_intarr(&u->fs, &x->pos_fs, "scrollpos", 2, x->pos, u)
      || init_prop_intarr(&u->fs, &x->expand_fs, "expand", 2, x->expand, u)
      || ui_init_place(&x->place, 0)
      || arr_memcpy(&x->place.sticky.buf, 5, 0, 5, "tblr") < 0) {
    free(x);
    return -1;
  }
  x->pos_fs.p.update = x->expand_fs.p.update = ui_prop_update_default;
  x->place.fs.name = "_";
  add_file(&x->c.fs_items, &x->place.fs);
  u->ops = &scroll_ops;
  u->data = x;
  x->c.u = u;
  add_file(&u->fs, &x->c.fs_items);
  return 0;
}
