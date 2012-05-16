#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <Imlib2.h>

#include "util.h"
#include "9p.h"
#include "fs.h"
#include "fsutil.h"
#include "fstypes.h"
#include "geom.h"
#include "event.h"
#include "ctl.h"
#include "surface.h"
#include "view.h"
#include "client.h"
#include "prop.h"
#include "ui.h"

static void create_place(struct p9_connection *c);

struct p9_fs container_fs = {
  .create = create_place
};

static void
rm_place(struct file *f)
{
  struct uiobj_place *up = (struct uiobj_place *)f;
  if (!up)
    return;
  if (up->path.buf)
    free(up->path.buf);
  if (up->sticky.buf)
    free(up->sticky.buf);
}

static struct client *
get_place_client(struct uiobj_place *up)
{
  if (up->obj)
    return UIOBJ_CLIENT(up->obj);
  if (up->fs.parent && (FSTYPE(*up->fs.parent) == FS_UIPLACE_DIR)
      && up->fs.parent->parent)
    return UIOBJ_CLIENT((struct uiobj *)up->fs.parent->parent);
  if (up->fs.parent && (FSTYPE(*up->fs.parent) == FS_VIEW))
    return ((struct view *)up->fs.parent)->c;
  return 0;
}

static void
place_uiobj(struct uiobj_place *up, struct uiobj *u)
{
  struct uiobj_parent *p, *p1;

  log_printf(LOG_UI, "place_uiobj u: %p\n", u);
  if (u) {
    p = (struct uiobj_parent *)malloc(sizeof(struct uiobj_parent));
    if (!p)
      die("Cannot allocate memory");
    log_printf(LOG_DBG, "u->fs_places.aux.p: %p\n", u->fs_places.aux.p);
    p->place = up;
    p->prev = 0;
    p1 = (struct uiobj_parent *)u->fs_places.aux.p;
    p->next = p1;
    if (p1)
      p1->prev = p;
    u->fs_places.aux.p = p;
  }
  up->obj = u;
}

static void
unplace_uiobj(struct uiobj_place *up)
{
  struct uiobj_parent *p;

  if (!up->obj)
    return;
  p = (struct uiobj_parent *)up->obj->fs_places.aux.p;
  for (; p && p->place != up; p = p->next) {}
  if (!p)
    return;
  if (p->prev)
    p->prev->next = p->next;
  else
    up->obj->fs_places.aux.p = p->next;
  if (p->next)
    p->next->prev = p->prev;
  free(p);
  up->obj = 0;
}

static void
upd_placement(struct prop *p)
{
  struct uiobj_place *up = (struct uiobj_place *)p->aux;
  struct client *c;
  struct file *vf;
  c = get_place_client(up);
  if (!c)
    return;
  for (vf = c->fs_views.child; vf; vf = vf->next)
    ((struct view *)vf)->flags |= VIEW_IS_DIRTY;
}

static void
upd_path(struct prop *p)
{
  struct prop_buf *pb = (struct prop_buf *)p;
  struct uiobj_place *up = (struct uiobj_place *)p->aux;
  struct client *c;
  struct uiobj *prevu;
  struct arr *buf;

  log_printf(LOG_UI, "upd_path\n");
  c = get_place_client(up);
  prevu = up->obj;
  if (up->obj)
    unplace_uiobj(up);
  log_printf(LOG_UI, "  client: %p\n", c);
  if (!c)
    return;
  up->obj = 0;
  buf = pb->buf;
  if (buf && buf->used > 0) {
    for (; buf->b[buf->used - 1] <= ' '; --buf->used) {}
    log_printf(LOG_UI, "  buf: '%.*s'\n", buf->used, buf->b);
    place_uiobj(up, (struct uiobj *)find_file_path(c->ui, buf->used, buf->b));
    if (!up->obj)
      pb->buf->used = 0;
  }
  if (up->obj != prevu)
    upd_placement(p);
}

static void
create_place(struct p9_connection *c)
{
  struct uiobj *u;
  struct uiobj_place *up;
  int r;

  log_printf(LOG_UI, ";; create place\n");

  if (!c->t.pfid->file)
    return;

  u = (struct uiobj *)((struct file *)c->t.pfid->file)->aux.p;
  if (!u->data)
    return;
  up = (struct uiobj_place *)malloc(sizeof(struct uiobj_place));
  if (!up) {
    P9_SET_STR(c->r.ename, "Cannot allocate memory");
    return;
  }
  memset(up, 0, sizeof(*up));

  up->fs.name = strndup(c->t.name, c->t.name_len);
  up->fs.owns_name = 1;
  if (!up->fs.name) {
    P9_SET_STR(c->r.ename, "Cannot allocate memory");
    free(up);
    return;
  }
  r = init_prop_buf(&up->fs, &up->path, "path", 0, "", 0, up)
      | init_prop_buf(&up->fs, &up->sticky, "sticky", 4, 0, 1, up)
      | init_prop_rect(&up->fs, &up->padding, "padding", up)
      | init_prop_rect(&up->fs, &up->place, "place", up);
  up->place.r[1] = up->place.r[3] = 1;
  up->place.r[0] = 1;
  up->place.r[2] = -1;

  if (r) {
    P9_SET_STR(c->r.ename, "Cannot allocate memory");
    free(up->fs.name);
    free(up);
    return;
  }
  up->path.p.update = upd_path;
  up->sticky.p.update = upd_placement;
  up->padding.p.update = upd_placement;

  up->fs.mode = 0500 | P9_DMDIR;
  up->fs.qpath = new_qid(0);
  up->fs.rm = rm_place;
  up->fs.aux.p = up;
  add_file(&((struct uiobj_container *)u->data)->fs_items, &up->fs);
  log_printf(LOG_UI, ";; done creating place '%s'\n", up->fs.name);

  update_uiobj(u);
}

void
init_container_items(struct uiobj_container *c, char *name)
{
  memset(&c->fs_items, 0, sizeof(c->fs_items));
  c->fs_items.name = name;
  c->fs_items.owns_name = 0;
  c->fs_items.mode = 0700 | P9_DMDIR;
  c->fs_items.qpath = new_qid(FS_UIPLACE_DIR);
  c->fs_items.fs = &container_fs;
}

static void
rm_places_fid(struct p9_fid *fid)
{
  if (fid->aux)
    free(fid->aux);
}

static void
places_open(struct p9_connection *c)
{
  struct uiobj *u;
  struct client *cl;
  struct p9_fid *fid = c->t.pfid;
  struct arr *buf = 0;
  struct uiobj_parent *par;

  u = (struct uiobj *)((char *)fid->file - offsetof(struct uiobj, fs_places));
  cl = (struct client *)u->fs.aux.p;
  par = (struct uiobj_parent *)u->fs_places.aux.p;
  for (; par; par = par->next) {
    if (file_path(&buf, &u->fs, cl->ui))
      die("Cannot allocate memory");
    if (buf && arr_memcpy(&buf, 16, buf->used, 1, "\n") < 0)
      die("Cannot allocate memory");
  }
  fid->aux = buf;
  c->t.pfid->rm = rm_places_fid;
}

static void
places_read(struct p9_connection *c)
{
  struct arr *buf = (struct arr *)c->t.pfid->aux;
  if (buf)
    read_buf_fn(c, buf->used, buf->b);
}

struct p9_fs places_fs = {
  .open = places_open,
  .read = places_read
};
