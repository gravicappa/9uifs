#include <stdlib.h>
#include <string.h>

#include "util.h"
#include "draw.h"
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

enum button_state {
  BTN_NORMAL,
  BTN_PRESSED
};

struct uiobj_label {
  struct prop_buf text;
  struct prop_buf font;
  struct prop_int fg;
  int state;
  unsigned int pressed_ms;
};

static void
rm_uilabel(struct file *f)
{
  ui_rm_uiobj(f);
}

static void
draw(struct uiobj *u, struct view *v)
{
  unsigned int fg, bg;
  struct uiobj_label *x = (struct uiobj_label *)u->data;

  bg = u->bg.i;
  fg = x->fg.i;

  if (bg && 0xff000000)
    fill_rect(v->blit.img, u->g.r[0], u->g.r[1], u->g.r[2], u->g.r[3], bg);
  if (fg && 0xff000000 && x->text.buf)
    draw_utf8(v->blit.img, u->g.r[0], u->g.r[1], fg, 0, x->text.buf->b);
}

static void
update_size(struct uiobj *u)
{
  struct uiobj_label *x = (struct uiobj_label *)u->data;
  int w = 0, h = 0;
  if (x->text.buf)
    get_utf8_size(0, x->text.buf->b, &w, &h);
  log_printf(LOG_UI, ">> label.update_size [%d %d]\n", w, h);
  u->reqsize[0] = w;
  u->reqsize[1] = h;
}

static void
resize(struct uiobj *u)
{
  log_printf(LOG_UI, ">> label.resize [%d %d %d %d]\n",
             u->g.r[0], u->g.r[1], u->g.r[2], u->g.r[3]);
  log_printf(LOG_UI, "     reqsize: [%d %d]\n", u->reqsize[0], u->reqsize[1]);
}

static struct uiobj_ops label_ops = {
  .draw = draw,
  .resize = resize,
  .update_size = update_size
};

int
init_uilabel(struct uiobj *u)
{
  struct uiobj_label *x;

  u->data = 0;
  x = (struct uiobj_label *)malloc(sizeof(struct uiobj_label));
  if (!x)
    return -1;
  memset(x, 0, sizeof(struct uiobj_label));

  if (init_prop_buf(&u->fs, &x->text, "text", 0, "", 0, u)
      || init_prop_colour(&u->fs, &x->fg, "foreground", 0, u)
      || init_prop_buf(&u->fs, &x->font, "font", 0, "", 0, u)) {
    free(x);
    return -1;
  }
  x->text.p.update = ui_default_prop_update;
  x->font.p.update = ui_default_prop_update;
  u->ops = &label_ops;
  u->data = x;
  u->fs.rm = rm_uilabel;
  u->bg.i = DEFAULT_LABEL_BG;
  x->fg.i = DEFAULT_LABEL_FG;
  return 0;
}

static void
draw_btn(struct uiobj *u, struct view *v)
{
  unsigned int fg, bg, frame;
  struct uiobj_label *b = (struct uiobj_label *)u->data;

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
    if (b->pressed_ms > 0 && cur_time_ms - b->pressed_ms > BTN_PRESS_TIME_MS)
      b->state = BTN_NORMAL;
    break;
  }

  if (bg && 0xff000000)
    fill_rect(v->blit.img, u->g.r[0], u->g.r[1], u->g.r[2], u->g.r[3], bg);
  if (frame && 0xff000000)
    draw_rect(v->blit.img, u->g.r[0], u->g.r[1], u->g.r[2], u->g.r[3], frame);
  if (fg && 0xff000000 && b->text.buf)
    draw_utf8(v->blit.img, u->g.r[0], u->g.r[1], fg, 0, b->text.buf->b);
}

static void
press_button(struct uiobj *u, int by_kbd)
{
  struct uiobj_label *b = (struct uiobj_label *)u->data;

  if (b->state == BTN_PRESSED)
    put_ui_event(&u->client->ev, u->client, "press_button\t$o\n", u);
  if (by_kbd)
    b->pressed_ms = cur_time_ms;
}

static int
on_btn_key(struct uiobj *u, int type, int keysym, int mod, unsigned int uni)
{
  struct uiobj_label *b = (struct uiobj_label *)u->data;
  if (keysym == '\r') {
    if (type) {
      b->state = BTN_PRESSED;
      b->pressed_ms = 0;
    } else
      press_button(u, 1);
    return 1;
  }
  return 0;
}

static int
on_btn_press_pointer(struct uiobj *u, int type, int x, int y, int btn)
{
  struct uiobj_label *b = (struct uiobj_label *)u->data;
  if (type) {
    b->state = BTN_PRESSED;
    b->pressed_ms = 0;
  } else {
    if (b->state == BTN_PRESSED)
      press_button(u, 0);
    b->state = BTN_NORMAL;
  }
  return 1;
}

static int
on_btn_inout_pointer(struct uiobj *u, int inside)
{
  struct uiobj_label *x = (struct uiobj_label *)u->data;
  if (!inside)
    x->state = BTN_NORMAL;
  return 1;
}

static struct uiobj_ops btn_ops = {
  .draw = draw_btn,
  .resize = resize,
  .update_size = update_size,
  .on_press_pointer = on_btn_press_pointer,
  .on_key = on_btn_key,
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
