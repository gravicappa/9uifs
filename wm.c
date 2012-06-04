#include <SDL/SDL.h>
#include <Imlib2.h>

#include "util.h"
#include "9p.h"
#include "fs.h"
#include "geom.h"
#include "client.h"
#include "event.h"
#include "ctl.h"
#include "surface.h"
#include "prop.h"
#include "view.h"
#include "screen.h"

void
wm_new_view_geom(struct rect *r)
{
  r->x = 0;
  r->y = 0;
  r->w = screen.front->w;
  r->h = screen.front->h;
}

void
wm_on_create_view(struct view *v)
{
  selected_view = v;
  moveresize_view(selected_view, 0, 0, screen.front->w, screen.front->h);
}

void
wm_view_size_request(struct view *v)
{
  v->g.r[0] = 0;
  v->g.r[1] = 0;
  v->g.r[2] = screen.front->w;
  v->g.r[3] = screen.front->h;
}

void
wm_on_rm_view(struct view *v)
{
  struct client *c = v->c;
  c->selected_view = ((v == (struct view *)c->fs_views.child)
                      ? (struct view *)v->fs.next
                      : (struct view *)c->fs_views.child);
  moveresize_view(c->selected_view, 0, 0, screen.front->w, screen.front->h);
}
