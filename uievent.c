#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>
#include <time.h>

#include "util.h"
#include "api.h"
#include "9p.h"
#include "fs.h"
#include "fsutil.h"
#include "fstypes.h"
#include "bus.h"
#include "ctl.h"
#include "backend.h"
#include "surface.h"
#include "prop.h"
#include "uiobj.h"
#include "ui.h"
#include "client.h"

static struct evmask {
  char *s;
  int mask;
} evmask[] = {
  {"kbd", UI_KBD_EV},
  {"ptr_move", UI_MOVE_PTR_EV},
  {"ptr_updown", UI_UPDOWN_PTR_EV},
  {"ptr_intersect", UI_PTR_INTERSECT_EV},
  {"resize", UI_RESIZE_EV},
  {0}
};

static int
kbd_ev(struct uiobj *u, struct input_event *ev)
{
  char *type;

  if (!u)
    return 0;

  if (u->ops->on_input && u->ops->on_input(u, ev))
    return 1;

  type = (ev->type == IN_KEY_DOWN) ? "d" : "u";
  if (u && u->flags & UI_KBD_EV) {
    struct ev_arg evargs[] = {
      {ev_str, {.s = "key"}},
      {ev_str, {.s = type}},
      {ev_uint, {.u = ev->key}},
      {ev_uint, {.u = ev->state}},
      {ev_uint, {.u = ev->unicode}},
      {ev_uiobj, {.o = u}},
      {0}
    };
    static const char *tags[] = {bus_ch_kbd, bus_ch_all, 0};
    put_event(u->client->bus, tags, evargs);
  }
  return 0;
}

static int
inside_uiobj(int x, int y, struct uiobj *u)
{
  int *r = u->viewport.r;
  return (x >= r[0] && y >= r[1] && x <= (r[0] + r[2]) && y <= (r[1] + r[3]));
}

struct input_context {
  struct input_event *ev;
  struct uiobj *u;
  struct uiobj *over;
};

static int
uiobj_input(struct uiobj *u, struct input_event *ev)
{
  switch (ev->type) {
  case IN_PTR_MOVE:
    if (u->flags & UI_MOVE_PTR_EV) {
      struct ev_arg evargs[] = {
        {ev_str, {.s = "ptr"}},
        {ev_str, {.s = "m"}},
        {ev_uint, {.u = ev->id}},
        {ev_uint, {.u = ev->x}},
        {ev_uint, {.u = ev->y}},
        {ev_int, {.i = ev->dx}},
        {ev_int, {.i = ev->dy}},
        {ev_uint, {.u = ev->state}},
        {ev_uiobj, {.o = u}},
        {0}
      };
      static const char *tags[] = {bus_ch_ptr, 0};
      put_event(u->client->bus, tags, evargs);
      return 1;
    }
    break;
  case IN_PTR_UP:
  case IN_PTR_DOWN:
    if (u->flags & UI_UPDOWN_PTR_EV) {
      struct ev_arg evargs[] = {
        {ev_str, {.s = "ptr"}},
        {ev_str, {.s = (ev->type == IN_PTR_UP) ? "u" : "d"}},
        {ev_uint, {.u = ev->id}},
        {ev_uint, {.u = ev->x}},
        {ev_uint, {.u = ev->y}},
        {ev_uint, {.u = ev->key}},
        {ev_uiobj, {.o = u}},
        {0}
      };
      static const char *tags[] = {bus_ch_ptr, 0};
      put_event(u->client->bus, tags, evargs);
      return 1;
    }
    break;
  default:;
  }
#if 0
  log_printf(LOG_UI, "on-input '%s' %p\n", u->f.name, u->ops->on_input);
  /*return 1;*/
#endif
  return u->ops->on_input && u->ops->on_input(u, ev);
}

static int
input_event_fn(struct uiplace *up, void *aux)
{
  struct input_context *ctx = (struct input_context *)aux;
  struct input_event *ev = ctx->ev;
  struct uiobj *u = up->obj;

#if 0
  log_printf(LOG_UI, "input_event_fn %s\n", u ? u->f.name : "(nil)");
#endif

  if (!(u && inside_uiobj(ev->x, ev->y, u)))
    return 1;

  if (!ctx->over) {
#if 0
    log_printf(LOG_UI, "input_event_fn over <- %s\n",
               u ? u->f.name : "(nil)");
#endif
    ctx->over = u;
  }
#if 0
  log_printf(LOG_UI, "input_event_fn on-input %s\n", u ? u->f.name : "(nil)");
#endif

  if (uiobj_input(u, ev)) {
    ctx->u = u;
    return 0;
  }
  return 1;
}

static struct uiobj *
process_ptr_exit(struct uiobj *obj, int x, int y)
{
  struct uiobj *last = 0;
  struct ev_arg evargs[] = {
    {ev_str, {.s = "ptr"}},
    {ev_str, {.s = "out"}},
    {ev_uiobj},
    {0}
  };
  static const char *tags[] = {bus_ch_ui, bus_ch_all, 0};
  while (obj) {
    last = obj;
    if (inside_uiobj(x, y, obj))
      break;
    else if (obj->ops->on_ptr_intersect)
      obj->ops->on_ptr_intersect(obj, 0);
    if (obj->flags & UI_PTR_INTERSECT_EV) {
      evargs[2].x.o = obj;
      put_event(obj->client->bus, tags, evargs);
    }
    if  (obj->place && obj->place->parent)
      obj = obj->place->parent->obj;
    else
      obj = 0;
  }
  return last;
}

static void
process_ptr_enter(struct uiobj *u, struct uiobj *prev, int x, int y)
{
  struct uiobj *obj;
  struct ev_arg evargs[] = {
    {ev_str, {.s = "ptr"}},
    {ev_str, {.s = "in"}},
    {ev_uiobj},
    {0}
  };
  static const char *tags[] = {bus_ch_ui, bus_ch_all, 0};

  if (prev == u)
    return;
  obj = u;
  while (obj && obj != prev) {
    if (!inside_uiobj(x, y, obj))
      break;
    else if (obj->ops->on_ptr_intersect)
      obj->ops->on_ptr_intersect(obj, 1);
    if (obj->flags & UI_PTR_INTERSECT_EV) {
      evargs[2].x.o = obj;
      put_event(obj->client->bus, tags, evargs);
    }
    if  (obj->place && obj->place->parent)
      obj = obj->place->parent->obj;
    else
      obj = 0;
  }
}

static int
ptr_ev(struct uiobj *u, struct input_event *ev)
{
  struct input_context ctx = {ev, 0, 0};
  struct uiobj *t, *obj;
  struct uiplace *up;

  if (ui_grabbed)
    return uiobj_input(ui_grabbed, ev);

  t = process_ptr_exit(ui_pointed, ev->x, ev->y);
  if (!ui_desktop->obj)
    return 0;
  if ((ui_desktop->obj->flags & UI_DIRTY) || !t)
    walk_ui_tree(ui_desktop, 0, input_event_fn, &ctx);
  else {
    walk_ui_tree(t->place, 0, input_event_fn, &ctx);
    if (t->place)
      for (up = t->place->parent; up && up->obj; up = up->parent) {
        obj = up->obj;
        if (uiobj_input(obj, ev))
          break;
      }
    process_ptr_enter(ctx.over, t, ev->x, ev->y);
  }
  ui_pointed = ctx.over;

  if (ctx.u)
    ui_focused = ctx.u;
  return 0;
}

int
uifs_input_event(struct input_event *ev)
{
  struct uiobj *u;

  return 0;
  u = (ui_grabbed) ? ui_grabbed : ((ui_focused) ? ui_focused : ui_pointed);
  switch (ev->type) {
  case IN_PTR_MOVE:
  case IN_PTR_UP:
  case IN_PTR_DOWN:
    return ptr_ev(u, ev);
  case IN_KEY_UP:
  case IN_KEY_DOWN:
    return kbd_ev(u, ev);
  default: return 0;
  }
}

void
evmask_open(struct p9_connection *con)
{
  struct p9_fid *fid = con->t.pfid;
  struct uiobj *u = containerof(fid->file, struct uiobj, f_evfilter);
  struct arr *buf = 0;
  int i, n, off;

  fid->aux = 0;
  fid->rm = rm_fid_aux;

  if (P9_WRITE_MODE(fid->open_mode) && (fid->open_mode & P9_OTRUNC))
    return;
  for (i = 0; evmask[i].s; ++i)
    if (u->flags & evmask[i].mask) {
      n = strlen(evmask[i].s);
      off = (buf) ? buf->used : 0;
      if (arr_memcpy(&buf, 8, -1, n + 1, 0) < 0) {
        P9_SET_STR(con->r.ename, "out of memory");
        return;
      }
      memcpy(buf->b + off, evmask[i].s, n);
      buf->b[off + n] = '\n';
    }
  fid->aux = buf;
}

void
evmask_read(struct p9_connection *con)
{
  struct arr *buf = con->t.pfid->aux;
  if (buf)
    read_str_fn(con, buf->used, buf->b);
}

void
evmask_write(struct p9_connection *con)
{
  write_buf_fn(con, 16, (struct arr **)&con->t.pfid->aux);
}

void
evmask_clunk(struct p9_connection *con)
{
  struct p9_fid *fid = con->t.pfid;
  struct uiobj *u = containerof(fid->file, struct uiobj, f_evfilter);
  char *args, *arg;
  int i, flags = u->flags;

  if (!fid->aux)
    return;

  for (i = 0; evmask[i].s; ++i)
    flags &= ~evmask[i].mask;
  args = ((struct arr *)fid->aux)->b;
  while ((arg = next_arg(&args)))
    for (i = 0; evmask[i].s; ++i)
      if (!strcmp(evmask[i].s, arg))
        flags |= evmask[i].mask;
  u->flags = flags;
}

static struct p9_fs eventmask_fs = {
  .open = evmask_open,
  .read = evmask_read,
  .write = evmask_write,
  .clunk = evmask_clunk,
};

void
ui_init_evfilter(struct file *f)
{
  f->name = "evfilter";
  f->mode = 0600;
  f->qpath = new_qid(0);
  f->fs = &eventmask_fs;
}
