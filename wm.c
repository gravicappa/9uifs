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
wm_on_rm_view(struct view *v)
{
  struct client *c = v->c;
  selected_view = (v == c->views) ? v->next : c->views;
  moveresize_view(selected_view, 0, 0, screen.front->w, screen.front->h);
}