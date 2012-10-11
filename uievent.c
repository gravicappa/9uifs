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
#include "geom.h"
#include "event.h"
#include "ctl.h"
#include "draw.h"
#include "surface.h"
#include "prop.h"
#include "view.h"
#include "uiobj.h"
#include "ui.h"
#include "client.h"

struct event_desc {
  char *fmt;
  struct view *v;
  struct uiobj *u;
  int kbd_type;
  int kbd_keysym;
  unsigned int kbd_unicode;
  int ptr_type;
  int ptr_x;
  int ptr_y;
  int ptr_state;
  int ptr_btn;
};

static int
event_length(struct client *c, const char *fmt, va_list args)
{
  int i, mode, x, n = 0;
  struct view *v;
  struct uiobj *u;

  for (i = 0, mode = 0; fmt[i]; ++i) {
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
          n += strlen(v->fs.name);
          break;
        case 'o':
          u = va_arg(args, struct uiobj *);
          n += file_path_len(&u->fs, c->ui);
          break;
        default: n++; break;
      }
      mode = 0;
    }
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

  for (i = 0, j = 0, mode = 0; fmt[i]; ++i) {
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
          n = sprintf(b + j, "%s", v->fs.name);
          j += n;
          break;
        case 'o':
          u = va_arg(args, struct uiobj *);
          n = file_path(size - j, buf + j, &u->fs, c->ui);
          j += n;
          break;
        default: b[j++] = fmt[i]; break;
      }
      mode = 0;
    }
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

  if (u && u->ops->on_key && u->ops->on_key(u, ev))
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

struct pointer_finder {
  int x;
  int y;
  struct uiobj *u;
};

static int
inside_uiobj(int x, int y, struct uiobj *u)
{
  int *r;
  if (!u)
    return 0;
  r = u->viewport.r;
  return (x >= r[0] && y >= r[1] && x <= (r[0] + r[2]) && y <= (r[1] + r[3]));
}

static int
find_uiobj_fn(struct uiplace *up, void *aux)
{
  struct pointer_finder *finder = (struct pointer_finder *)aux;
  if (inside_uiobj(finder->x, finder->y, up->obj)) {
    finder->u = up->obj;
    return 1;
  }
  return 0;
}

static struct uiobj *
find_uiobj_by_xy(struct view *v, int x, int y)
{
  struct pointer_finder f = {x, y, 0};
  struct uiplace *up = (struct uiplace *)v->uiplace;
  walk_view_tree(up, find_uiobj_fn, 0, &f);
  return f.u;
}

static void
pointer_enter_exit(struct view *v, struct uiobj *sel, int x, int y)
{
  struct uiobj *pointed = (struct uiobj *)v->uipointed;
  if (pointed != sel) {
    if (pointed) {
      if (pointed->ops->on_inout_pointer)
        pointed->ops->on_inout_pointer(pointed, 0);
      /* TODO: global pointer exit event */
    }
    if (sel) {
      if (sel->ops->on_inout_pointer)
        sel->ops->on_inout_pointer(sel, 1);
      /* TODO: global pointer enter event */
    }
  }
}

struct input_context {
  struct input_event *ev;
  struct uiobj *u;
  struct uiobj *over;
  struct view *v;
};

static int
input_event_after_fn(struct uiplace *up, void *aux)
{
  struct input_context *ctx = (struct input_context *)aux;
  struct input_event *ev = ctx->ev;
  struct uiobj *u = up->obj;

  if (!inside_uiobj(ev->x, ev->y, u))
    return 1;

  if (!ctx->over)
    ctx->over = u;

  if (0) pointer_enter_exit(ctx->v, u, ev->x, ev->y);
  if (u->ops->on_input && u->ops->on_input(u, ev)) {
    ctx->u = u;
    return 0;
  }
  return 1;
}

static void
enter_exit(struct uiobj *prev, struct uiobj *u, int x, int y)
{
  struct uiplace *up, *upprev;
  struct uiobj *obj, *last;

  if (prev == u)
    return;
  obj = prev;
  while (obj) {
    last = obj;
    if (inside_uiobj(x, y, obj))
      break;
    else if (obj->ops->on_inout_pointer)
      obj->ops->on_inout_pointer(obj, 0);
    if  (obj->parent && obj->parent->parent)
      obj = obj->parent->parent->obj;
    else
      obj = 0;
  }
  obj = u;
  while (obj && obj != last) {
    if (!inside_uiobj(x, y, obj))
      break;
    else if (obj->ops->on_inout_pointer)
      obj->ops->on_inout_pointer(obj, 1);
    if  (obj->parent && obj->parent->parent)
      obj = obj->parent->parent->obj;
    else
      obj = 0;
  }
}

int
ui_pointer_press(struct view *v, struct input_event *ev)
{
  struct input_context ctx = {ev, 0, 0, v};
  int type;

  walk_view_tree((struct uiplace *)v->uiplace, 0, input_event_after_fn, &ctx);

  log_printf(LOG_UI, "ui_pointer_press ev.type: %d u: %s\n", ev->type,
             (ctx.u) ? ctx.u->fs.name : "(nil)");

  enter_exit((struct uiobj *)v->uipointed, ctx.over, ev->x, ev->y);
  v->uipointed = (struct file *)ctx.over;

  if (ctx.u)
    v->uisel = &ctx.u->fs;

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
    v->uipointed = &ctx.u->fs;
    break;
  default:;
  }
  return 0;
}

int
ui_pointer_move(struct view *v, struct input_event *ev)
{
  return ui_pointer_press(v, ev);
}
