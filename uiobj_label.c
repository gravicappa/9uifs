#include <stdlib.h>
#include <string.h>

#include "util.h"
#include "api.h"
#include "frontend.h"
#include "text.h"
#include "9p.h"
#include "fs.h"
#include "prop.h"
#include "bus.h"
#include "ui.h"
#include "uiobj.h"
#include "config.h"
#include "client.h"
#include "font.h"
#include "dirty.h"

enum button_state {
  BTN_NORMAL,
  BTN_PRESSED,
  BTN_ACTIVE
};

struct uiobj_label {
  struct prop_buf text;
  struct prop_buf font_str;
  struct prop_int fg;
  struct prop_int caret;
  struct prop_int allow_multi;
  UFont font;
  int state;
  int caret_rect[4];
  int padding[4];
  unsigned int pressed_ms;
};

static void
rm_uilabel(struct file *f)
{
  struct uiobj *u = (struct uiobj *)f;
  struct uiobj_label *lb = u->data;
  if (lb->font)
    free_font(lb->font);
  ui_rm_uiobj(f);
}

static void
update_font(struct prop *font_prop)
{
  struct uiobj *u = font_prop->aux;
  struct prop_buf *p = (struct prop_buf *)font_prop;
  struct uiobj_label *lb = u->data;
  UFont fn;

  if (!lb || !p->buf || !(fn = font_from_str(p->buf->b)))
    return;
  free_font(lb->font);
  lb->font = fn;
  ui_prop_upd(font_prop);
}

static void
draw(struct uiobj *u, struct uicontext *uc)
{
  unsigned int fg, bg, cursbg = 0x7faf00af, cursfg = 0xfff0000;
  struct uiobj_label *lb = u->data;
  int *r = u->g.r, *pad = lb->padding;

  bg = u->bg.i;
  fg = lb->fg.i;

  if (bg & 0xff000000)
    draw_rect(screen_image, r[0], r[1], r[2], r[3], 0, bg);
  if (lb->caret.i >= 0 && lb->caret_rect[2] && lb->caret_rect[3])
    draw_rect(screen_image, r[0] + pad[0] + lb->caret_rect[0],
              r[1] + pad[1] + lb->caret_rect[1], lb->caret_rect[2],
              lb->caret_rect[3], cursfg, cursbg);
  if ((fg & 0xff000000) && lb->text.buf)
    multi_draw_utf8(screen_image, r[0] + pad[0], r[1] + pad[1], fg, lb->font,
                    lb->text.buf->used - 1, lb->text.buf->b);
}

static void
update_size(struct uiobj *u)
{
  struct uiobj_label *lb = u->data;
  int w = 0, h = 0;
  struct arr *buf = lb->text.buf;
  if (buf)
    multi_get_utf8_size(lb->font, buf->used - 1, buf->b, &w, &h);
  u->reqsize[0] = w + lb->padding[0] + lb->padding[2];
  u->reqsize[1] = h + lb->padding[1] + lb->padding[3];
}

static void
update_text(struct prop *p)
{
  struct uiobj *u = p->aux;
  update_size(u);
  mark_uiobj_dirty(u);
  if (u->reqsize[0] > u->g.r[2] || u->reqsize[1] > u->g.r[3])
    u->flags |= UI_DIRTY;
  ui_enqueue_update(u);
}

static struct uiobj_ops label_ops = {
  .draw = draw,
  .update_size = update_size
};

int
init_uilabel(struct uiobj *u)
{
  struct uiobj_label *lb;

  u->data = 0;
  lb = calloc(1, sizeof(struct uiobj_label));
  if (!lb)
    return -1;

  if (init_prop_buf(&u->f, &lb->text, "text", 0, "", 0, u)
      || init_prop_colour(&u->f, &lb->fg, "foreground", 0, u)
      || init_prop_buf(&u->f, &lb->font_str, "font", 0, "", 0, u)) {
    free(lb);
    return -1;
  }
  lb->fg.p.update = ui_prop_updvis;
  lb->text.p.update = update_text;
  lb->font_str.p.update = update_font;
  u->ops = &label_ops;
  u->data = lb;
  u->f.rm = rm_uilabel;
  u->bg.i = DEF_LABEL_BG;
  lb->fg.i = DEF_LABEL_FG;
  lb->caret.i = -1;
  return 0;
}

static void
update_btn(struct uiobj *u)
{
  struct uiobj_label *b = u->data;
  unsigned int t;

  if (b->state == BTN_PRESSED) {
    t = cur_time_ms - b->pressed_ms ;
    if (b->pressed_ms > 0 && t > BTN_PRESS_TIME_MS) {
      b->state = BTN_NORMAL;
    }
  }
  mark_uiobj_dirty(u);
}

static void
draw_btn(struct uiobj *u, struct uicontext *uc)
{
  unsigned int fg, bg, frame;
  struct uiobj_label *b = u->data;
  int x, y, ux, uy, *r = u->g.r;

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
  draw_rect(screen_image, r[0], r[1], r[2], r[3], frame, bg);
  if (0) draw_rect(screen_image, r[0] + 2, r[1] + 2, r[2] - 4, r[3] - 4, frame, bg);
  if ((fg & 0xff000000) && b->text.buf) {
    x = r[0] + ((r[2] - u->reqsize[0]) >> 1) + b->padding[0];
    y = r[1] + ((r[3] - u->reqsize[1]) >> 1) + b->padding[1];
    ux = x - b->padding[0];
    uy = r[1] + r[3] - b->padding[1];
    draw_line(screen_image, ux, uy, ux + u->reqsize[0], uy, frame);
    multi_draw_utf8(screen_image, x, y, fg, b->font, b->text.buf->used - 1,
                    b->text.buf->b);
  }
}

static void
press_button(struct uiobj *u, int by_kbd)
{
  struct uiobj_label *b = u->data;

  if (b->state == BTN_PRESSED) {
    struct ev_arg ev[] = {
      {ev_str, {.s = "press_button"}},
      {ev_uiobj, {.o = u}},
      {0}
    };
    put_event(u->client->bus, bus_ch_ev, ev);
  }
  if (by_kbd)
    b->pressed_ms = cur_time_ms;
}

static int
on_btn_input(struct uiobj *u, struct input_event *ev)
{
  struct uiobj_label *b = u->data;
  int prevstate = b->state;
  if (0) log_printf(LOG_UI, "on btn input '%s'\n", u->f.name);
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
  if (prevstate != b->state)
    ui_enqueue_update(u);
  return 1;
}

static int
on_btn_ptr_intersect(struct uiobj *u, int inside)
{
  struct uiobj_label *lb = u->data;
  if (!inside && (lb->state != BTN_NORMAL)) {
    ui_enqueue_update(u);
    lb->state = BTN_NORMAL;
  }
  return 1;
}

static struct uiobj_ops btn_ops = {
  .update = update_btn,
  .draw = draw_btn,
  .update_size = update_size,
  .on_input = on_btn_input,
  .on_ptr_intersect = on_btn_ptr_intersect
};

int
init_uibutton(struct uiobj *u)
{
  struct uiobj_label *lb;
  if (init_uilabel(u))
    return -1;
  lb = u->data;
  lb->padding[0] = lb->padding[1] = lb->padding[2] = lb->padding[3] = 4;
  lb->state = BTN_NORMAL;
  u->ops = &btn_ops;
  u->bg.i = DEF_BTN_BG;
  lb->fg.i = DEF_BTN_FG;
  return 0;
}

static void
put_entry_caret(struct uiobj *u, int caret)
{
  struct uiobj_label *lb = u->data;
  int *r = lb->caret_rect;
  if (!lb->text.buf) {
    lb->caret.i = -1;
    r[2] = r[3] = 0;
    return;
  }
  lb->caret.i = ((caret >= -1) ? ((caret <= lb->text.buf->used)
                                  ? caret : lb->text.buf->used)
                 : -1);
  multi_get_utf8_info_at_index(lb->font, lb->text.buf->used - 1,
                               lb->text.buf->b, lb->caret.i, r, r + 1,
                               r + 2, r + 3);
  mark_uiobj_dirty(u);
  ui_enqueue_update(u);
}

static void
update_caret(struct prop *p)
{
  struct uiobj *u = p->aux;
  struct uiobj_label *lb = u->data;
  put_entry_caret(u, lb->caret.i);
}

static void
on_entry_input(struct uiobj *u, struct input_event *ev)
{
  struct uiobj_label *lb = u->data;
  int c = -1, *r = lb->caret_rect;
  switch (ev->type) {
  case IN_PTR_DOWN:
    c = multi_get_utf8_info_at_point(lb->font,
                                     lb->text.buf->used - 1, lb->text.buf->b,
                                     ev->x - u->g.r[0], ev->y - u->g.r[1],
                                     r + 0, r + 1, r + 2, r + 3);
    if (c >= 0)
      lb->caret.i = c;
    mark_uiobj_dirty(u);
    ui_enqueue_update(u);
    break;
  case IN_KEY_UP:
    switch (ev->key) {
    case 273: /* sdl_up */
      break;
    case 274: /* sdl_down */
      break;
    case 276: /* sdl_left */
      if (lb->caret.i > 0)
        c = lb->caret.i - 1;
      break;
    case 275: /* sdl_right */
      if (lb->text.buf && lb->caret.i < lb->text.buf->used - 1)
          c = lb->caret.i + 1;
      break;
    }
    break;
  default:;
  }
  if (c >= 0 && lb->caret.i != c)
    put_entry_caret(u, c);
}

static struct uiobj_ops entry_ops = {
  .draw = draw,
  .update_size = update_size,
  .on_input = on_entry_input,
};

int
init_uientry(struct uiobj *u)
{
  struct uiobj_label *lb;
  if (init_uilabel(u))
    return -1;
  lb = u->data;
  if (init_prop_int(&u->f, &lb->caret, "caret", -1, u)
      || init_prop_int(&u->f, &lb->allow_multi, "allow_multi", 0, u)) {
    free(lb);
    return -1;
  }
  u->ops = &entry_ops;
  lb->caret.p.update = update_caret;
  return 0;
}
