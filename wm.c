#include "util.h"
#include "9p.h"
#include "fs.h"
#include "event.h"
#include "client.h"
#include "ctl.h"
#include "draw.h"
#include "surface.h"
#include "prop.h"
#include "view.h"
#include "input.h"
#include "ui.h"

struct view *wm_selected_view = 0;
struct uiobj *grabbed_ptr_uiobj = 0;
struct uiobj *grabbed_kbd_uiobj = 0;
struct view *grabbed_ptr_view = 0;
struct view *grabbed_kbd_view = 0;

void
wm_new_view_geom(int *r)
{
  struct screen *s = default_screen();
  r[0] = 0;
  r[1] = 0;
  r[2] = s->w;
  r[3] = s->h;
}

void
wm_on_create_view(struct view *v)
{
  struct screen *s = default_screen();
  wm_selected_view = v;
  moveresize_view(wm_selected_view, 0, 0, s->w, s->h);
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
  wm_selected_view = ((v == (struct view *)c->f_views.child)
                      ? (struct view *)v->f.next
                      : (struct view *)c->f_views.child);
  moveresize_view(wm_selected_view, 0, 0, s->w, s->h);
}

static int
inside_view(int x, int y, struct view *v)
{
  int *r = v->g.r;
  return (x >= r[0] && y >= r[1] && x <= (r[0] + r[2]) && y <= (r[1] + r[3]));
}

int
wm_on_input(struct input_event *ev)
{
  struct client *c;
  struct view *prevsel, *v = wm_selected_view;
  struct file *vf;
  struct uiobj *u = 0;

  switch (ev->type) {
  case IN_PTR_MOVE:
    v = grabbed_ptr_view;
    u = grabbed_ptr_uiobj;
    if (!grabbed_ptr_view) {
      prevsel = wm_selected_view;
      wm_selected_view = 0;
      for (c = clients; c; c = c->next)
        for (vf = c->f_views.child; vf; vf = vf->next) {
          v = (struct view *)vf;
          if (inside_view(ev->x, ev->y, v)) {
            wm_selected_view = v;
            break;
          }
        }
      v = wm_selected_view;
      if (prevsel != wm_selected_view) {
        if (prevsel && (prevsel->flags & VIEW_FOCUS_EV)
            && prevsel->c->ev.listeners) {
          struct ev_fmt evfmt[] = {
            {ev_str, {.s = "unfocus"}},
            {ev_view, {.v = prevsel}},
            {0}
          };
          put_event(c, &c->ev, evfmt);
        }
        if (wm_selected_view && (wm_selected_view->flags & VIEW_FOCUS_EV)) {
          struct ev_fmt evfmt[] = {
            {ev_str, {.s = "focus"}},
            {ev_view, {.v = wm_selected_view}},
            {0}
          };
          put_event(c, &c->ev, evfmt);
        }
      }
    }
    break;
  case IN_PTR_UP:
  case IN_PTR_DOWN:
    v = (grabbed_ptr_view) ? grabbed_ptr_view : wm_selected_view;
    u = grabbed_ptr_uiobj;
    break;
  case IN_KEY_UP:
  case IN_KEY_DOWN:
    v = (grabbed_kbd_view) ? grabbed_kbd_view : wm_selected_view;
    u = grabbed_ptr_uiobj;
    break;
  }
  if (v)
    handle_view_input(v, u, ev);
  return 0;
}

void
wm_grab_ptr(struct view *v, struct uiobj *u)
{
  grabbed_ptr_view = v;
  grabbed_ptr_uiobj = u;
}

void
wm_ungrab_ptr()
{
  grabbed_ptr_view = 0;
  grabbed_ptr_uiobj = 0;
}

void
wm_grab_kbd(struct view *v, struct uiobj *u)
{
  grabbed_kbd_view = v;
  grabbed_kbd_uiobj = u;
}

void
wm_ungrab_kbd()
{
  grabbed_kbd_view = 0;
  grabbed_kbd_uiobj = 0;
}
