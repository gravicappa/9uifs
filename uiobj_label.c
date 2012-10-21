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

  if (bg && 0xff000000)
    fill_rect(blit->img, r[0], r[1], r[2], r[3], bg);
  if (fg && 0xff000000 && x->text.buf)
    multi_draw_utf8(blit->img, r[0], r[1], fg, x->font,
                    x->text.buf->used - 1, x->text.buf->b);
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
  x->text.p.update = x->fg.p.update = ui_prop_update_default;
  x->font_str.p.update = update_font;
  u->ops = &label_ops;
  u->data = x;
  u->f.rm = rm_uilabel;
  u->bg.i = DEFAULT_LABEL_BG;
  x->fg.i = DEFAULT_LABEL_FG;
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
      log_printf(LOG_UI, "dirty uiobj %s : %d\n", u->f.name, __LINE__);
    }
  }
}

static void
draw_btn(struct uiobj *u, struct uicontext *uc)
{
  unsigned int fg, bg, frame;
  struct uiobj_label *b = (struct uiobj_label *)u->data;
  struct surface *blit = &uc->v->blit;

  switch (b->state) {
  case BTN_NORMAL:
    bg = u->bg.i;
    fg = b->fg.i;
    frame = DEFAULT_BTN_FG;
    break;

  case BTN_PRESSED:
    bg = DEFAULT_BTN_PRESSED_BG;
    fg = DEFAULT_BTN_PRESSED_FG;
    frame = DEFAULT_BTN_PRESSED_FG;
    break;
  }

  if (bg && 0xff000000)
    fill_rect(blit->img, u->g.r[0], u->g.r[1], u->g.r[2], u->g.r[3], bg);
  if (frame && 0xff000000)
    draw_rect(blit->img, u->g.r[0], u->g.r[1], u->g.r[2], u->g.r[3], frame);
  if (fg && 0xff000000 && b->text.buf)
    multi_draw_utf8(blit->img, u->g.r[0], u->g.r[1], fg, b->font,
                    b->text.buf->used - 1, b->text.buf->b);
}

static void
press_button(struct uiobj *u, int by_kbd)
{
  struct uiobj_label *b = (struct uiobj_label *)u->data;

  if (b->state == BTN_PRESSED)
    put_ui_event(&u->client->ev, u->client, "press_button\t$o\n", u);
  if (by_kbd)
    b->pressed_ms = cur_time_ms;
  u->flags |= UI_DIRTY;
  log_printf(LOG_UI, "dirty uiobj %s : %d\n", u->f.name, __LINE__);
}

static int
on_btn_input(struct uiobj *u, struct input_event *ev)
{
  struct uiobj_label *b = (struct uiobj_label *)u->data;
  log_printf(LOG_UI, "btn input event\n", u->f.name);
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
  log_printf(LOG_UI, "dirty uiobj %s : %d\n", u->f.name, __LINE__);
  return 1;
}

static int
on_btn_inout_pointer(struct uiobj *u, int inside)
{
  struct uiobj_label *x = (struct uiobj_label *)u->data;
#if 0
  log_printf(LOG_UI, "btn %s %s\n", u->f.name, (inside) ? "in" : "out");
#endif
  if (!inside) {
    x->state = BTN_NORMAL;
    u->flags |= UI_DIRTY;
#if 0
    log_printf(LOG_UI, "dirty uiobj %s : %d\n", u->f.name, __LINE__);
#endif
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
  u->flags = UI_KBD_EV | UI_PRESS_PTR_EV;
  u->ops = &btn_ops;
  u->bg.i = DEFAULT_BTN_BG;
  x->fg.i = DEFAULT_BTN_FG;
  return 0;
}
