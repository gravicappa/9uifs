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

struct uiobj_label {
  struct prop_buf text;
  struct prop_buf font;
  struct prop_int fg;
};

static void
rm_uilabel(struct file *f)
{
  ui_rm_uiobj(f);
}

static void
draw(struct uiobj *u, struct view *v)
{
  unsigned int c;
  struct uiobj_label *x = (struct uiobj_label *)u->data;

  c = u->bg.i;
  if (c && 0xff000000)
    fill_rect(v->blit.img, u->g.r[0], u->g.r[1], u->g.r[2], u->g.r[3], c);
  c = x->fg.i;
  if (c && 0xff000000 && x->text.buf)
    draw_utf8(v->blit.img, u->g.r[0], u->g.r[1], c, 0, x->text.buf->b);
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
  return 0;
}
