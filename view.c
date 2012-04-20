#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <SDL/SDL.h>
#include <Imlib2.h>

#include "util.h"
#include "9p.h"
#include "fs.h"
#include "fsutil.h"
#include "ctl.h"
#include "geom.h"
#include "surface.h"
#include "client.h"
#include "event.h"
#include "view.h"
#include "screen.h"
#include "prop.h"
#include "ui.h"
#include "wm.h"

extern int scr_w;
extern int scr_h;

void
rm_view(struct file *f)
{
  struct view *v = (struct view *)f->aux.p;
  wm_on_rm_view(v);
  free(v);
}

void
views_create(struct p9_connection *c)
{
  struct client *cl = (struct client *)c;
  struct view *v;
  struct rect r;

  log_printf(LOG_DBG, "; views_create '%.*s' %p\n", c->t.name_len, c->t.name,
             cl);

  if (!(c->t.perm & P9_DMDIR)) {
    P9_SET_STR(c->r.ename, "wrong view create perm");
    return;
  }
  wm_new_view_geom(&r);
  v = mk_view(r.x, r.y, r.w, r.h);
  if (!v) {
    P9_SET_STR(c->r.ename, "cannot create view");
    return;
  }
  v->c = cl;
  v->fs.name = strndup(c->t.name, c->t.name_len);
  if (!v->fs.name) {
    P9_SET_STR(c->r.ename, "cannot create view");
    rm_view(&v->fs);
    return;
  }
  v->fs.owns_name = 1;
  v->next = cl->views;
  cl->views = v;
  add_file(&cl->fs_views, &v->fs);
  wm_on_create_view(v);
  resp_file_create(c, &v->fs);
}

static struct view *
get_view(struct p9_connection *c)
{
  struct p9_fid *fid = c->t.pfid;
  struct file *f = (struct file *)fid->file;
  return (struct view *)f->aux.p;
}

void
view_visible_write(struct p9_connection *c)
{
  struct view *v = get_view(c);
  v->visible = write_bool_fn(c, v->visible);
}

void
view_visible_read(struct p9_connection *c)
{
  struct view *v = get_view(c);
  read_bool_fn(c, v->visible);
}

struct p9_fs fs_views = {
  .create = views_create,
};

struct p9_fs fs_view_visible = {
  .write = view_visible_write,
  .read = view_visible_read
};

struct view *
mk_view(int x, int y, int w, int h)
{
  struct view *v;

  v = (struct view *)malloc(sizeof(struct view));
  if (!v)
    die("Cannot allocate memory");
  memset(v, 0, sizeof(*v));
  v->g.x = x;
  v->g.y = y;
  v->g.w = w;
  v->g.h = h;
  if (init_surface(&v->blit, w, h)) {
    free(v);
    return 0;
  }
  v->fs.mode = 0500 | P9_DMDIR;
  v->fs.qpath = ++qid_cnt;
  v->fs.aux.p = v;
  v->fs.rm = rm_view;

  v->ev.f.name = "event";
  v->ev.f.mode = 0400;
  v->ev.f.qpath = ++qid_cnt;
  v->ev.f.aux.p = &v->ev;
  v->ev.f.fs = &fs_event;
  add_file(&v->fs, &v->ev.f);

  v->ev_pointer.f.name = "pointer";
  v->ev_pointer.f.mode = 0400;
  v->ev_pointer.f.qpath = ++qid_cnt;
  v->ev_pointer.f.aux.p = &v->ev_pointer;
  v->ev_pointer.f.fs = &fs_event;
  add_file(&v->fs, &v->ev_pointer.f);

  v->ev_keyboard.f.name = "keyboard";
  v->ev_keyboard.f.mode = 0400;
  v->ev_keyboard.f.qpath = ++qid_cnt;
  v->ev_keyboard.f.aux.p = &v->ev_keyboard;
  v->ev_keyboard.f.fs = &fs_event;
  add_file(&v->fs, &v->ev_keyboard.f);

  v->fs_visible.name = "visible";
  v->fs_visible.mode = 0600;
  v->fs_visible.qpath = ++qid_cnt;
  v->fs_visible.aux.p = v;
  v->fs_visible.fs = &fs_view_visible;
  add_file(&v->fs, &v->fs_visible);

  v->fs_geometry.name = "geometry";
  v->fs_geometry.mode = 0600;
  v->fs_geometry.qpath = ++qid_cnt;
  v->fs_geometry.aux.p = v;
  add_file(&v->fs, &v->fs_geometry);

  v->blit.fs.name = "blit";
  add_file(&v->fs, &v->blit.fs);

  v->fs_gl.name = "gl";
  v->fs_gl.mode = 0700 | P9_DMDIR;
  v->fs_gl.qpath = ++qid_cnt;
  v->fs_gl.aux.p = v;
  add_file(&v->fs, &v->fs_gl);

  v->fs_canvas.name = "canvas";
  v->fs_canvas.mode = 0700 | P9_DMDIR;
  v->fs_canvas.qpath = ++qid_cnt;
  v->fs_canvas.aux.p = v;
  add_file(&v->fs, &v->fs_canvas);
#if 0
  v->ui = mk_ui(&v->fs, "ui");
  if (v->ui)
    v->ui->v = v;
#endif
  return v;
}

void
moveresize_view(struct view *v, int x, int y, int w, int h)
{
  char buf[64];
  int len;

  v->g.x = x;
  v->g.y = y;
  v->g.w = w;
  v->g.h = h;
  resize_surface(&v->blit, w, h);

  len = snprintf(buf, sizeof(buf), "geom %u %u %u %u\n", x, y, w, h);
  put_event(v->c, &v->ev, len, buf);
}

void
draw_view(struct view *v)
{
  imlib_context_set_image(screen.imlib);
  imlib_context_set_anti_alias(1);
  imlib_context_set_blend(0);
  imlib_blend_image_onto_image(v->blit.img, 0, 0, 0, v->blit.w, v->blit.h,
                               v->g.x, v->g.y, v->g.w, v->g.h);
}
