#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>
#include <time.h>

#include "util.h"
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
ui_keyboard(struct view *v, int type, int keysym, int mod,
            unsigned int unicode)
{
  struct uiobj *u = (struct uiobj *)v->uisel;

  if (u && u->ops->on_key && u->ops->on_key(u, type, keysym, mod, unicode))
    return 1;
  if (v->flags & VIEW_KBD_EV)
    put_ui_event(&v->ev, v->c, "key\t$n\t$n\t$n\t$u\t$o\n", type, keysym,
                 mod, unicode, u);
  if (u && u->flags & UI_KBD_EV)
    put_ui_event(&v->c->ev, v->c, "key\t$n\t$n\t$n\t$u\t$v\t$o\n", type,
                 keysym, mod, unicode, v, u);
  return 0;
}

struct pointer_finder {
  int x;
  int y;
  struct uiobj *u;
};

static int
find_uiobj_fn(struct uiplace *up, struct view *v, void *aux)
{
  struct pointer_finder *finder = (struct pointer_finder *)aux;
  struct uiobj *u = up->obj;
  int *r, x, y;
  if (u) {
    r = u->g.r;
    x = finder->x;
    y = finder->y;
    if (x >= r[0] && y >= r[1] && x <= (r[0] + r[2]) && y <= (r[1] + r[3])) {
      finder->u = u;
      return 1;
    }
  }
  return 0;
}

static struct uiobj *
find_uiobj_by_xy(struct view *v, int x, int y)
{
  struct pointer_finder f = {x, y, 0};
  struct uiplace *up = (struct uiplace *)v->uiplace;
  walk_view_tree(up, v, &f, find_uiobj_fn, 0);
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

int
ui_pointer_move(struct view *v, int x, int y, int state)
{
  struct uiobj *sel = find_uiobj_by_xy(v, x, y);
  pointer_enter_exit(v, sel, x, y);
  if (sel && sel->ops->on_move_pointer)
    sel->ops->on_move_pointer(sel, x, y, state);
  v->uipointed = (struct file *)sel;
  return 0;
}

int
ui_pointer_press(struct view *v, int type, int x, int y, int btn)
{
  struct uiobj *sel = find_uiobj_by_xy(v, x, y);

  pointer_enter_exit(v, sel, x, y);
  v->uisel = (struct file *)sel;
  if (sel && sel->ops->on_press_pointer
      && sel->ops->on_press_pointer(sel, type, x, y, btn))
    return 1;

  if (sel) {
    if (v->flags & VIEW_PRESS_PTR_EV)
      put_ui_event(&v->ev, v->c, "press_ptr\t$n\t$n\t$n\t$n\t$o\n",
                   type, x, y, btn, sel);
    if (sel->flags & UI_PRESS_PTR_EV)
      put_ui_event(&v->c->ev, v->c, "press_ptr\t$n\t$n\t$n\t$n\t$v\t$o\n",
                   type, x, y, btn, v, sel);
    return 1;
  }
  return 0;
}
