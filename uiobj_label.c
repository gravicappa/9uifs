#include <stdlib.h>
#include <string.h>

#include "util.h"
#include "input.h"
#include "draw.h"
#include "text.h"
#include "9p.h"
#include "fs.h"
#include "prop.h"
#include "event.h"
#include "ctl.h"
#include "surface.h"
#include "view.h"
#include "ui.h"
#include "uiobj.h"
#include "config.h"
#include "client.h"
#include "font.h"

enum button_state {
  BTN_NORMAL,
  BTN_PRESSED
};

struct uiobj_label {
  struct prop_buf text;
  struct prop_buf font_str;
  struct prop_int fg;
  UFont font;
  int state;
  unsigned int pressed_ms;
};

static void
rm_uilabel(struct file *f)
{
  ui_rm_uiobj(f);
}

static void
update_font(struct prop *font_prop)
{
  struct uiobj *u = font_prop->aux;
  struct prop_buf *p = (struct prop_buf *)font_prop;
  struct uiobj_label *x = (struct uiobj_label *)u->data;
  UFont fn;

  if (!x || !p->buf || !(fn = font_from_str(p->buf->b)))
    return;
  free_font(x->font);
  x->font = fn;
  ui_prop_update_default(font_prop);
}

static void
draw(struct uiobj *u, struct uicontext *uc)
{
  unsigned int fg, bg;
  struct uiobj_label *x = (struct uiobj_label *)u->data;
  struct surface *blit = &uc->v->blit;
  int *r = u->g.r;

  bg = u->bg.i;
  fg = x->fg.i;

  if (bg & 0xff000000)
    draw_rect(blit->img, r[0], r[1], r[2], r[3], 0, bg);
  if ((fg & 0xff000000) && x->text.buf)
    multi_draw_utf8(blit->img, r[0], r[1], fg, x->font,
                    x->text.buf->used - 1, x->text.buf->b);
  mark_dirty_rect(r);
}

static void
update_size(struct uiobj *u)
{
  struct uiobj_label *x = (struct uiobj_label *)u->data;
  int w = 0, h = 0;
  struct arr *buf = x->text.buf;
  if (buf)
    multi_get_utf8_size(x->font, buf->used - 1, buf->b, &w, &h);
  u->reqsize[0] = w;
  u->reqsize[1] = h;
}

static void
update_text(struct prop *p)
{
  struct uiobj *u = p->aux;
  update_size(u);
  u->flags |= UI_DIRTY;
  if (u->reqsize[0] > u->g.r[2] || u->reqsize[1] > u->g.r[3])
    if (u->parent)
      ui_propagate_dirty(u->parent);
}

static struct uiobj_ops label_ops = {
  .draw = draw,
  .update_size = update_size
};

int
init_uilabel(struct uiobj *u)
{
  struct uiobj_label *x;

  u->data = 0;
  x = (struct uiobj_label *)calloc(1, sizeof(struct uiobj_label));
  if (!x)
    return -1;

  if (init_prop_buf(&u->f, &x->text, "text", 0, "", 0, u)
      || init_prop_colour(&u->f, &x->fg, "foreground", 0, u)
      || init_prop_buf(&u->f, &x->font_str, "font", 0, "", 0, u)) {
    free(x);
    return -1;
  }
  x->fg.p.update = ui_prop_update_default;
  x->text.p.update = update_text;
  x->font_str.p.update = update_font;
  u->ops = &label_ops;
  u->data = x;
  u->f.rm = rm_uilabel;
  u->bg.i = DEF_LABEL_BG;
  x->fg.i = DEF_LABEL_FG;
  return 0;
}

static void
update_btn(struct uiobj *u, struct uicontext *uc)
{
  struct uiobj_label *b = (struct uiobj_label *)u->data;
  unsigned int t;

  if (b->state == BTN_PRESSED) {
    t = cur_time_ms - b->pressed_ms ;
    if (b->pressed_ms > 0 && t > BTN_PRESS_TIME_MS) {
      b->state = BTN_NORMAL;
      u->flags |= UI_DIRTY;
    }
    ui_enqueue_update(u);
  }
}

static void
draw_btn(struct uiobj *u, struct uicontext *uc)
{
  unsigned int fg, bg, frame;
  struct uiobj_label *b = (struct uiobj_label *)u->data;
  struct surface *blit = &uc->v->blit;
  int *r = u->g.r, x, y;

  switch (b->state) {
  case BTN_NORMAL:
    bg = u->bg.i;
    fg = b->fg.i;
    frame = DEF_BTN_FG;
    break;

  case BTN_PRESSED:
    bg = DEF_BTN_PRESSED_BG;
    fg = DEF_BTN_PRESSED_FG;
    frame = DEF_BTN_PRESSED_FG;
    break;
  }

  draw_rect(blit->img, r[0], r[1], r[2], r[3], frame, bg);
  if ((fg & 0xff000000) && b->text.buf) {
    x = r[0] + ((r[2] - u->reqsize[0]) >> 1);
    y = r[1] + ((r[3] - u->reqsize[1]) >> 1);
    multi_draw_utf8(blit->img, x, y, fg, b->font, b->text.buf->used - 1,
                    b->text.buf->b);
  }
  mark_dirty_rect(r);
}

static void
press_button(struct uiobj *u, int by_kbd)
{
  struct uiobj_label *b = (struct uiobj_label *)u->data;

  if (b->state == BTN_PRESSED) {
    struct ev_fmt evfmt[] = {
      {ev_str, {.s = "press_button"}},
      {ev_uiobj, {.o = u}},
      {0}
    };
    put_event(u->client, &u->client->ev, evfmt);
  }
  if (by_kbd)
    b->pressed_ms = cur_time_ms;
  u->flags |= UI_DIRTY;
}

static int
on_btn_input(struct uiobj *u, struct input_event *ev)
{
  struct uiobj_label *b = (struct uiobj_label *)u->data;
  switch (ev->type) {
  case IN_PTR_DOWN:
    b->state = BTN_PRESSED;
    b->pressed_ms = 0;
    break;
  case IN_PTR_UP:
    if (b->state == BTN_PRESSED)
      press_button(u, 0);
    b->state = BTN_NORMAL;
    break;
  case IN_KEY_DOWN:
    if (!(ev->key == '\n' || ev->key == '\r'))
      return 0;
    b->state = BTN_PRESSED;
    b->pressed_ms = 0;
    break;
  case IN_KEY_UP:
    if (!(ev->key == '\n' || ev->key == '\r'))
      return 0;
    press_button(u, 1);
    break;
  default: return 0;
  }
  u->flags |= UI_DIRTY;
  ui_enqueue_update(u);
  return 1;
}

static int
on_btn_inout_pointer(struct uiobj *u, int inside)
{
  struct uiobj_label *x = (struct uiobj_label *)u->data;
  if (!inside) {
    x->state = BTN_NORMAL;
    u->flags |= UI_DIRTY;
  }
  return 1;
}

static struct uiobj_ops btn_ops = {
  .update = update_btn,
  .draw = draw_btn,
  .update_size = update_size,
  .on_input = on_btn_input,
  .on_inout_pointer = on_btn_inout_pointer
};

int
init_uibutton(struct uiobj *u)
{
  struct uiobj_label *x;
  if (init_uilabel(u))
    return -1;
  x = (struct uiobj_label *)u->data;
  x->state = BTN_NORMAL;
  u->ops = &btn_ops;
  u->bg.i = DEF_BTN_BG;
  x->fg.i = DEF_BTN_FG;
  return 0;
}
