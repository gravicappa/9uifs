#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>

#include "util.h"
#include "9p.h"
#include "fs.h"
#include "fsutil.h"
#include "fstypes.h"
#include "ctl.h"
#include "draw.h"
#include "surface.h"
#include "event.h"
#include "client.h"
#include "prop.h"
#include "view.h"
#include "ui.h"
#include "wm.h"
#include "config.h"

extern int scr_w;
extern int scr_h;

void
rm_view(struct file *f)
{
  struct view *v = (struct view *)f;
  wm_on_rm_view(v);
  free(v);
}

void
views_create(struct p9_connection *c)
{
  struct client *cl = (struct client *)c;
  struct view *v;
  int r[4];
  int res;

  if (!(c->t.perm & P9_DMDIR)) {
    P9_SET_STR(c->r.ename, "wrong view create perm");
    return;
  }
  wm_new_view_geom(r);
  v = mk_view(r[0], r[1], r[2], r[3], cl);
  if (!v) {
    P9_SET_STR(c->r.ename, "cannot create view");
    return;
  }
  v->f.name = strndup(c->t.name, c->t.name_len);
  if (!v->f.name) {
    P9_SET_STR(c->r.ename, "cannot create view");
    rm_view(&v->f);
    return;
  }
  v->f.owns_name = 1;
  res = init_prop_rect(&v->f, &v->g, "g", 1, v);
  if (res) {
    P9_SET_STR(c->r.ename, "cannot create view");
    rm_view(&v->f);
    return;
  }
  v->g.p.f.mode = 0400;
  add_file(&cl->f_views, &v->f);
  wm_on_create_view(v);
  resp_file_create(c, &v->f);
}

void
view_visible_write(struct p9_connection *c)
{
  struct view *v = containerof(c->t.pfid->file, struct view, f_visible);
  if (write_bool_fn(c, v->flags & VIEW_VISIBLE))
    v->flags |= (VIEW_VISIBLE | VIEW_DIRTY);
  else
    v->flags &= ~VIEW_VISIBLE;
}

void
view_visible_read(struct p9_connection *c)
{
  struct view *v = containerof(c->t.pfid->file, struct view, f_visible);
  read_bool_fn(c, v->flags & VIEW_VISIBLE);
}

struct p9_fs fs_views = {
  .create = views_create,
};

struct p9_fs fs_view_visible = {
  .write = view_visible_write,
  .read = view_visible_read
};

static void
update_blit(struct surface *s)
{
  struct view *v = s->aux;
  v->flags |= VIEW_DIRTY;
}

struct view *
mk_view(int x, int y, int w, int h, struct client *client)
{
  struct view *v;

  v = (struct view *)calloc(1, sizeof(struct view));
  if (!v)
    die("Cannot allocate memory");
  v->g.r[0] = x;
  v->g.r[1] = y;
  v->g.r[2] = w;
  v->g.r[3] = h;
  v->c = client;
  if (init_surface(&v->blit, w, h, v->c->images) || ui_init_uiplace(v)) {
    free(v);
    return 0;
  }
  draw_rect(v->blit.img, 0, 0, w, h, 0, DEFAULT_VIEW_BG);
  v->blit.aux = v;
  v->blit.update = update_blit;
  v->f.mode = 0500 | P9_DMDIR;
  v->f.qpath = new_qid(FS_VIEW);
  v->f.rm = rm_view;

  v->ev.f.name = "event";
  init_event(&v->ev);
  add_file(&v->f, &v->ev.f);

  v->ev_pointer.f.name = "pointer";
  init_event(&v->ev_pointer);
  add_file(&v->f, &v->ev_pointer.f);

  v->ev_keyboard.f.name = "keyboard";
  init_event(&v->ev_keyboard);
  add_file(&v->f, &v->ev_keyboard.f);

  v->f_visible.name = "visible";
  v->f_visible.mode = 0600;
  v->f_visible.qpath = new_qid(0);
  v->f_visible.fs = &fs_view_visible;
  add_file(&v->f, &v->f_visible);

  v->blit.f.name = "blit";
  add_file(&v->f, &v->blit.f);

  v->f_gl.name = "gles";
  v->f_gl.mode = 0700 | P9_DMDIR;
  v->f_gl.qpath = new_qid(0);
  add_file(&v->f, &v->f_gl);

  v->f_canvas.name = "canvas";
  v->f_canvas.mode = 0700 | P9_DMDIR;
  v->f_canvas.qpath = new_qid(0);
  add_file(&v->f, &v->f_canvas);

  v->uiplace->name = "uiplace";
  add_file(&v->f, v->uiplace);
  return v;
}

void
moveresize_view(struct view *v, int x, int y, int w, int h)
{
  char buf[64];
  int len;

  if (!v)
    return;
  v->g.r[0] = x;
  v->g.r[1] = y;
  v->g.r[2] = w;
  v->g.r[3] = h;
  resize_surface(&v->blit, w, h);
  v->flags |= VIEW_DIRTY;

  len = snprintf(buf, sizeof(buf), "geom %u %u %u %u\n", x, y, w, h);
  put_event_str(v->c, &v->ev, len, buf);
}

int
draw_view(struct view *v)
{
  struct screen *s = default_screen();
  if (ui_redraw_view(v) || (v->flags & VIEW_DIRTY)) {
    blit_image(s->blit, v->g.r[0], v->g.r[1], v->g.r[2], v->g.r[3],
               v->blit.img, 0, 0, v->blit.w, v->blit.h);
    return 1;
  }
  return 0;
}
