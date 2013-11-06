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
#include "backend.h"
#include "surface.h"
#include "prop.h"
#include "uiobj.h"
#include "ui.h"
#include "client.h"

static int
kbd_ev(struct uiobj *u, struct input_event *ev)
{
  static const char *tags_all[] = {bus_ch_kbd, bus_ch_all, 0};
  static const char *tags_uiobj[] = {bus_ch_kbd, bus_ch_all, bus_ch_ui, 0};
  static const char **tags[] = {tags_all, tags_uiobj};

  if (!u)
    return 0;

  if (u->ops->on_input && u->ops->on_input(u, ev))
    return 1;
  else {
    struct ev_arg evargs[] = {
      {ev_str, {.s = "key"}},
      {ev_str},
      {ev_uint, {.u = ev->key}},
      {ev_uint, {.u = ev->state}},
      {ev_uint, {.u = ev->unicode}},
      {ev_uiobj, {.o = u}},
      {0}
    };
    evargs[1].x.s = (ev->type == IN_KEY_DOWN) ? "d" : "u";
    put_event(u->client->bus, tags[(u->flags & UI_KBD_EV)], evargs);
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
      static const char *tags[] = {bus_ch_ptr, bus_ch_all, 0};
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
      static const char *tags[] = {bus_ch_ptr, bus_ch_all, 0};
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
