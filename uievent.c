#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>
#include <time.h>

#include "util.h"
#include "input.h"
#include "9p.h"
#include "fs.h"
#include "fsutil.h"
#include "fstypes.h"
#include "event.h"
#include "ctl.h"
#include "draw.h"
#include "surface.h"
#include "prop.h"
#include "view.h"
#include "uiobj.h"
#include "ui.h"
#include "client.h"

static int
event_length(struct client *c, const char *fmt, va_list args)
{
  int i, mode, x, n = 0;
  struct view *v;
  struct uiobj *u;

  for (i = 0, mode = 0; fmt[i]; ++i)
    if (!mode) {
      if (fmt[i] == '$')
        mode = 1;
      else
        n++;
    } else {
      switch (fmt[i]) {
        case 'n':
        case 'u':
          x = va_arg(args, int);
          if (abs(x) < 65536)
            n += 6;
          else
            n += 11;
          break;
        case 'v':
          v = va_arg(args, struct view *);
          n += strlen(v->f.name);
          break;
        case 'o':
          u = va_arg(args, struct uiobj *);
          n += file_path_len(&u->f, c->ui);
          break;
        default: n++; break;
      }
      mode = 0;
    }
  return n + 1;
}

int
put_ui_event(struct ev_pool *ev, struct client *c, const char *fmt, ...)
{
  va_list args, a;
  char buf[1024], *b = buf;
  int i, j, mode, n, size = sizeof(buf);
  struct view *v;
  struct uiobj *u;

  va_start(args, fmt);
  va_copy(a, args);
  n = event_length(c, fmt, a);
  if (n >= sizeof(buf)) {
    b = (char *)malloc(n);
    if (!b)
      return -1;
    size = n;
  }

  for (i = 0, j = 0, mode = 0; fmt[i]; ++i)
    if (!mode) {
      if (fmt[i] == '$')
        mode = 1;
      else
        b[j++] = fmt[i];
    } else {
      switch (fmt[i]) {
        case 'n':
          n = sprintf(b + j, "%d", va_arg(args, int));
          j += n;
          break;
        case 'u':
          n = sprintf(b + j, "%u", va_arg(args, unsigned int));
          j += n;
          break;
        case 'v':
          v = va_arg(args, struct view *);
          n = sprintf(b + j, "%s", v->f.name);
          j += n;
          break;
        case 'o':
          u = va_arg(args, struct uiobj *);
          n = file_path(size - j, buf + j, &u->f, c->ui);
          j += n;
          break;
        default: b[j++] = fmt[i]; break;
      }
      mode = 0;
    }
  put_event(c, ev, j, b);
  if (b != buf)
    free(b);
  va_end(args);
  return 0;
}

int
ui_keyboard(struct view *v, struct input_event *ev)
{
  struct uiobj *u = (struct uiobj *)v->uisel;
  int type;

  if (u && u->ops->on_input && u->ops->on_input(u, ev))
    return 1;

  type = (ev->type == IN_KEY_DOWN) ? 1 : 0;
  if (v->flags & VIEW_KBD_EV)
    put_ui_event(&v->ev, v->c, "key\t$n\t$n\t$n\t$u\t$o\n", type, ev->key,
                 ev->state, ev->unicode, u);
  if (u && u->flags & UI_KBD_EV)
    put_ui_event(&v->c->ev, v->c, "key\t$n\t$n\t$n\t$u\t$v\t$o\n", type,
                 ev->key, ev->state, ev->unicode, v, u);
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
  struct view *v;
};

static int
input_event_fn(struct uiplace *up, void *aux)
{
  struct input_context *ctx = (struct input_context *)aux;
  struct input_event *ev = ctx->ev;
  struct uiobj *u = up->obj;

  if (0)
    log_printf(LOG_UI, "input_event_fn %s\n", u ? u->f.name : "(nil)");

  if (!(u && inside_uiobj(ev->x, ev->y, u)))
    return 1;

  if (!ctx->over) {
    if (0)
      log_printf(LOG_UI, "input_event_fn over <- %s\n",
                 u ? u->f.name : "(nil)");
    ctx->over = u;
  }

  if (0)
    log_printf(LOG_UI, "input_event_fn on-input %s\n",
               u ? u->f.name : "(nil)");
  if (u->ops->on_input && u->ops->on_input(u, ev)) {
    ctx->u = u;
    return 0;
  }
  return 1;
}

static struct uiobj *
onexit(struct view *v, struct uiobj *obj, int x, int y)
{
  struct uiobj *last = 0;
  while (obj) {
    last = obj;
    if (inside_uiobj(x, y, obj))
      break;
    else if (obj->ops->on_inout_pointer)
      obj->ops->on_inout_pointer(obj, 0);
    if (obj->flags & UI_INOUT_EV)
      put_ui_event(&v->ev, v->c, "ptr_out\t$o\n", obj);
    if  (obj->parent && obj->parent->parent)
      obj = obj->parent->parent->obj;
    else
      obj = 0;
  }
  return last;
}

static void
onenter(struct view *v, struct uiobj *prev, struct uiobj *u, int x, int y)
{
  struct uiobj *obj;

  if (prev == u)
    return;
  obj = u;
  while (obj && obj != prev) {
    if (!inside_uiobj(x, y, obj))
      break;
    else if (obj->ops->on_inout_pointer)
      obj->ops->on_inout_pointer(obj, 1);
    if (obj->flags & UI_INOUT_EV)
      put_ui_event(&v->ev, v->c, "ptr_in\t$o\n", obj);
    if  (obj->parent && obj->parent->parent)
      obj = obj->parent->parent->obj;
    else
      obj = 0;
  }
}

int
ui_pointer_event(struct view *v, struct input_event *ev)
{
  struct input_context ctx = {ev, 0, 0, v};
  int type;
  struct uiobj *t, *obj;
  struct uiplace *up, *parent;

  t = onexit(v, (struct uiobj *)v->uipointed, ev->x, ev->y);
  if ((v->flags & VIEW_EV_DIRTY) || !t) {
    ui_walk_view_tree((struct uiplace *)v->uiplace, 0, input_event_fn, &ctx);
  } else {
    if (t->parent)
      parent = t->parent->parent;
    ui_walk_view_tree(t->parent, 0, input_event_fn, &ctx);
    if (t->parent)
      t->parent->parent = parent;
    if (t->parent && t->parent->obj) {
      up = t->parent->obj->parent;
      for (up = t->parent->obj->parent; up && up->obj; up = up->parent) {
        obj = up->obj;
        if (obj->ops->on_input && obj->ops->on_input(obj, ev))
          break;
      }
    }
    onenter(v, t, ctx.over, ev->x, ev->y);
  }
  v->flags &= ~VIEW_EV_DIRTY;
  v->uipointed = (struct file *)ctx.over;

  if (ctx.u)
    v->uisel = &ctx.u->f;

  switch (ev->type) {
  case IN_PTR_DOWN:
  case IN_PTR_UP:
    if (!ctx.u)
      return 0;
    type = (ev->type == IN_PTR_DOWN) ? 1 : 0;
    if (v->flags & VIEW_PRESS_PTR_EV)
      put_ui_event(&v->ev, v->c, "press_ptr\t$n\t$n\t$n\t$n\t$o\n",
                   type, ev->x, ev->y, ev->key, ctx.u);
    if (ctx.u->flags & UI_PRESS_PTR_EV)
      put_ui_event(&v->c->ev, v->c, "press_ptr\t$n\t$n\t$n\t$n\t$v\t$o\n",
                   type, ev->x, ev->y, ev->key, v, ctx.u);
    return 1;
  case IN_PTR_MOVE:
    break;
  default:;
  }
  return 0;
}

#if 0
void
evmask_open(struct p9_connection *con)
{
  ;
}

static struct p9_fs eventmask_fs = {
  .open = evmask_open,
  .read = evmask_read,
  .write = evmask_write,
  .clunk = evmask_clunk,
};
#endif
