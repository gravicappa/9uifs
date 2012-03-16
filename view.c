#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "util.h"
#include "9p.h"
#include "fs.h"
#include "fsutil.h"
#include "surface.h"
#include "client.h"
#include "view.h"

void
views_create(struct p9_connection *c)
{
  struct client *cl = (struct client *)c;
  struct view *v;

  if (!(c->t.perm & P9_DMDIR)) {
    P9_SET_STR(c->r.ename, "wrong view create perm");
    return;
  }
  v = mk_view(0, 0, 16, 16);
  if (!v) {
    P9_SET_STR(c->r.ename, "cannot create view");
    return;
  }
  v->fs.name = strndup(c->t.name, c->t.name_len);
  if (!v->fs.name) {
    P9_SET_STR(c->r.ename, "cannot create view");
    /* TODO: free created view */
    return;
  }
  v->fs.owns_name = 1;
  v->next = cl->views;
  cl->views = v;
}

void
rm_view(struct file *f)
{
  struct view *v = (struct view *)f->context.p;
}

static struct view *
get_view(struct p9_connection *c)
{
  struct p9_fid *fid = (struct p9_fid *)c->t.context;
  struct file *f = (struct file *)fid->context;
  return (struct view *)f->context.p;
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
    die("cannot allocate memory");
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
  v->fs.context.p = v;
  v->fs.rm = rm_view;

  v->fs_event.name = "event";
  v->fs_event.mode = 0400;
  v->fs_event.qpath = ++qid_cnt;
  v->fs_event.context.p = v;
  add_file(&v->fs, &v->fs_event);

  v->fs_visible.name = "visible";
  v->fs_visible.mode = 0600;
  v->fs_visible.qpath = ++qid_cnt;
  v->fs_visible.context.p = v;
  add_file(&v->fs, &v->fs_visible);

  v->fs_geometry.name = "geometry";
  v->fs_geometry.mode = 0600;
  v->fs_geometry.qpath = ++qid_cnt;
  v->fs_geometry.context.p = v;
  add_file(&v->fs, &v->fs_geometry);

  add_file(&v->blit.fs, &v->fs);

  v->fs_gl.name = "gl";
  v->fs_gl.mode = 0600;
  v->fs_gl.qpath = ++qid_cnt;
  v->fs_gl.context.p = v;
  add_file(&v->fs, &v->fs_gl);

  v->fs_canvas.name = "canvas";
  v->fs_canvas.mode = 0700;
  v->fs_canvas.qpath = ++qid_cnt;
  v->fs_canvas.context.p = v;
  add_file(&v->fs, &v->fs_canvas);

  v->fs_ui.name = "ui";
  v->fs_ui.mode = 0700;
  v->fs_ui.qpath = ++qid_cnt;
  v->fs_ui.context.p = v;
  add_file(&v->fs, &v->fs_ui);

  return v;
}
