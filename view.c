#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <SDL/SDL.h>
#include <Imlib2.h>

#include "util.h"
#include "9p.h"
#include "fs.h"
#include "fsutil.h"
#include "fstypes.h"
#include "ctl.h"
#include "geom.h"
#include "surface.h"
#include "client.h"
#include "event.h"
#include "prop.h"
#include "view.h"
#include "screen.h"
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
  int res;

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
  res = init_prop_rect(&v->fs, &v->g, "g", v);
  if (res) {
    P9_SET_STR(c->r.ename, "cannot create view");
    rm_view(&v->fs);
    return;
  }

  v->g.p.fs.mode = 0400;
  v->fs.owns_name = 1;
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
  if (write_bool_fn(c, v->flags & VIEW_IS_VISIBLE))
    v->flags |= VIEW_IS_VISIBLE;
  else
    v->flags &= ~VIEW_IS_VISIBLE;
}

void
view_visible_read(struct p9_connection *c)
{
  struct view *v = get_view(c);
  read_bool_fn(c, v->flags & VIEW_IS_VISIBLE);
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
  v->g.r[0] = x;
  v->g.r[1] = y;
  v->g.r[2] = w;
  v->g.r[3] = h;
  if (init_surface(&v->blit, w, h) || ui_init_uiplace(v)) {
    free(v);
    return 0;
  }
  v->fs.mode = 0500 | P9_DMDIR;
  v->fs.qpath = new_qid(FS_VIEW);
  v->fs.aux.p = v;
  v->fs.rm = rm_view;

  v->ev.f.name = "event";
  init_event(&v->ev);
  add_file(&v->fs, &v->ev.f);

  v->ev_pointer.f.name = "pointer";
  init_event(&v->ev_pointer);
  add_file(&v->fs, &v->ev_pointer.f);

  v->ev_keyboard.f.name = "keyboard";
  init_event(&v->ev_keyboard);
  add_file(&v->fs, &v->ev_keyboard.f);

  v->fs_visible.name = "visible";
  v->fs_visible.mode = 0600;
  v->fs_visible.qpath = new_qid(0);
  v->fs_visible.aux.p = v;
  v->fs_visible.fs = &fs_view_visible;
  add_file(&v->fs, &v->fs_visible);

  v->blit.fs.name = "blit";
  add_file(&v->fs, &v->blit.fs);

  v->fs_gl.name = "gl";
  v->fs_gl.mode = 0700 | P9_DMDIR;
  v->fs_gl.qpath = new_qid(0);
  v->fs_gl.aux.p = v;
  add_file(&v->fs, &v->fs_gl);

  v->fs_canvas.name = "canvas";
  v->fs_canvas.mode = 0700 | P9_DMDIR;
  v->fs_canvas.qpath = new_qid(0);
  v->fs_canvas.aux.p = v;
  add_file(&v->fs, &v->fs_canvas);

  v->uiplace->name = "uiplace";
  add_file(&v->fs, v->uiplace);
  return v;
}

void
moveresize_view(struct view *v, int x, int y, int w, int h)
{
  char buf[64];
  int len;

  v->g.r[0] = x;
  v->g.r[1] = y;
  v->g.r[2] = w;
  v->g.r[3] = h;
  resize_surface(&v->blit, w, h);
  v->flags |= VIEW_IS_DIRTY;

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
                               v->g.r[0], v->g.r[1], v->g.r[2], v->g.r[3]);
}

void
update_view(struct view *v)
{
  log_printf(LOG_UI, ">> update_view '%s'\n", v->fs.name);
  ui_update(v);
  v->flags &= ~VIEW_IS_DIRTY;
}
