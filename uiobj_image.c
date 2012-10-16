#include <stdlib.h>
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
#include "client.h"
#include "config.h"

struct uiobj_image {
  struct surface s;
  struct surface *cur;
  struct prop_buf path;
};

static void
update_blit(struct surface *s)
{
  struct uiobj *u = s->aux;
  if (u->parent)
    ui_propagate_dirty(u->parent);
}

static void
update_size(struct uiobj *u)
{
  struct uiobj_image *img = u->data;
  u->reqsize[0] = img->cur->w;
  u->reqsize[1] = img->cur->h;
}

static void
draw(struct uiobj *u, struct uicontext *ctx)
{
  struct uiobj_image *img = u->data;
  struct surface *blit = &ctx->v->blit;
  struct surface *s = img->cur;
  int *r = u->g.r;

  log_printf(LOG_UI, "image draw [%d %d %d %d]\n", r[0], r[1], r[2], r[3]);
  if (s->img)
    blit_image(blit->img, r[0], r[1], r[2], r[3], s->img, 0, 0, s->w, s->h);
}

static struct uiobj_ops image_ops = {
  .draw = draw,
  .update_size = update_size
};

int
init_uiimage(struct uiobj *u)
{
  struct uiobj_image *x;

  x = calloc(1, sizeof(struct uiobj_image));
  if (!x || init_prop_buf(&u->f, &x->path, "path", 0, "", 0, u)
      || init_surface(&x->s, 0, 0))
    return -1;
  x->s.f.name = "blit";
  x->s.update = update_blit;
  x->s.aux = u;
  x->cur = &x->s;
  u->data = x;
  u->ops = &image_ops;
  add_file(&u->f, &x->s.f);
  return 0;
}
