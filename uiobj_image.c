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
  struct surface *s = u->data;
  u->reqsize[0] = s->w;
  u->reqsize[1] = s->h;
}

static void
draw(struct uiobj *u, struct uicontext *ctx)
{
  struct surface *blit = &ctx->v->blit;
  struct surface *s = u->data;
  int *r = u->g.r;

  if (s->img)
    blit_image(blit->img, r[0], r[1], r[2], r[3], s->img, 0, 0, s->w, s->h);
}

static struct uiobj_ops image_ops = {
  .update_size = update_size,
  .draw = draw
};

int
init_uiimage(struct uiobj *u)
{
  struct surface *s;
  s = mk_surface(0, 0);
  if (!s)
    return -1;
  s->f.name = "blit";
  s->update = update_blit;
  s->aux = u;
  u->data = s;
  u->ops = &image_ops;
  add_file(&u->f, &s->f);
  return 0;
}
