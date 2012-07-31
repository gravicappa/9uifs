#include "util.h"
#include "9p.h"
#include "fs.h"
#include "geom.h"
#include "client.h"
#include "event.h"
#include "ctl.h"
#include "draw.h"
#include "surface.h"
#include "prop.h"
#include "view.h"

void
wm_new_view_geom(struct rect *r)
{
  struct screen *s = default_screen();
  r->x = 0;
  r->y = 0;
  r->w = s->w;
  r->h = s->h;
}

void
wm_on_create_view(struct view *v)
{
  struct screen *s = default_screen();
  selected_view = v;
  moveresize_view(selected_view, 0, 0, s->w, s->h);
}

void
wm_view_size_request(struct view *v)
{
  struct screen *s = default_screen();
  v->g.r[0] = 0;
  v->g.r[1] = 0;
  v->g.r[2] = s->w;
  v->g.r[3] = s->h;
}

void
wm_on_rm_view(struct view *v)
{
  struct client *c = v->c;
  struct screen *s = default_screen();
  c->selected_view = ((v == (struct view *)c->fs_views.child)
                      ? (struct view *)v->fs.next
                      : (struct view *)c->fs_views.child);
  moveresize_view(c->selected_view, 0, 0, s->w, s->h);
}
