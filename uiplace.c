#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <time.h>

#include "9p.h"
#include "fs.h"
#include "fstypes.h"
#include "fsutil.h"
#include "prop.h"
#include "ui.h"
#include "uiobj.h"
#include "util.h"
#include "client.h"

struct uiobj *
uiplace_container(struct uiplace *up)
{
  if (up->f.parent && FSTYPE(*up->f.parent) == FS_UIOBJ)
    return (struct uiobj *)up->f.parent;
  if (up->f.parent && (FSTYPE(*up->f.parent) == FS_UIPLACE_DIR)
      && up->f.parent->parent && FSTYPE(*up->f.parent->parent) == FS_UIOBJ)
    return (struct uiobj *)up->f.parent->parent;
  return 0;
}

static struct client *
get_place_client(struct uiplace *up)
{
  struct uiobj *u;

  if (up->obj)
    return up->obj->client;
  u = uiplace_container(up);
  if (u)
    return u->client;
  return 0;
}

static int
is_cycled(struct uiplace *up, struct uiobj *u)
{
  struct uiobj *x;
  while (up && (x = uiplace_container(up))) {
    if (x == u)
      return 1;
    up = x->place;
  }
  return 0;
}

static void
place_uiobj(struct uiplace *up, struct uiobj *u)
{
  struct file *f;
  if (!(u && up))
    return;
  if (is_cycled(up, u))
    return;
  if (u->place)
    u->place->obj = 0;
  u->place = up;
  up->obj = u;
  for (f = uiobj_children(u); f; f = f->next)
    ((struct uiplace *)f)->parent = up;
}

static void
unplace_uiobj(struct uiplace *up)
{
  struct file *f;
  if (!(up && up->obj))
    return;
  up->obj->place = 0;
  for (f = uiobj_children(up->obj); f; f = f->next)
    ((struct uiplace *)f)->parent = 0;
  up->obj = 0;
}

static void
rm_place_data(struct file *f)
{
  struct uiplace *up = (struct uiplace *)f;

  if (!up)
    return;
  if (up->obj) {
    unplace_uiobj(up);
    ui_enqueue_update(ui_desktop->obj);
  }
  if (up->sticky.buf)
    free(up->sticky.buf);
}

static void
rm_place(struct file *f)
{
  rm_place_data(f);
  free(f);
}


void
uiplace_prop_update_default(struct prop *p)
{
  struct uiplace *up = (struct uiplace *)p->aux;
  if (up)
    ui_propagate_dirty(up);
}

static void
path_open(struct p9_connection *con)
{
  struct uiplace *up;
  struct p9_fid *fid = con->t.pfid;
  int n;
  struct arr *buf = 0;

  up = containerof(fid->file, struct uiplace, f_path);
  fid->aux = 0;
  fid->rm = rm_fid_aux;

  if (up->obj && !(con->t.mode & P9_OTRUNC) && P9_READ_MODE(con->t.mode)) {
    n = file_path_len((struct file *)up->obj, up->obj->client->ui);
    if (arr_memcpy(&buf, n, 0, n, 0))
      die("Cannot allocate memory");
    file_path(n, buf->b, (struct file *)up->obj, up->obj->client->ui);
    fid->aux = buf;
  }
}

static void
path_write(struct p9_connection *con)
{
  write_buf_fn(con, 16, (struct arr **)&con->t.pfid->aux);
}

static void
path_read(struct p9_connection *con)
{
  struct arr *arr = con->t.pfid->aux;
  if (arr)
    read_str_fn(con, arr->used, arr->b);
}

static void
path_clunk(struct p9_connection *con)
{
  struct p9_fid *fid = con->t.pfid;
  struct uiplace *up;
  struct uiobj *prevu;
  struct client *client;
  struct arr *buf;

  if (!P9_WRITE_MODE(fid->open_mode))
    return;

  up = containerof(fid->file, struct uiplace, f_path);
  client = get_place_client(up);
  prevu = up->obj;
  if (up->obj)
    unplace_uiobj(up);
  up->obj = 0;
  buf = fid->aux;
  if (client && buf && buf->used > 0) {
    for (; buf->b[buf->used - 1] <= ' '; --buf->used) {}
    place_uiobj(up, (struct uiobj *)find_file(client->ui, buf->used, buf->b));
  }
  if (up->obj != prevu)
    ui_enqueue_update(ui_desktop->obj);
}

static struct p9_fs path_fs = {
  .open = path_open,
  .read = path_read,
  .write = path_write,
  .clunk = path_clunk
};

int
ui_init_place(struct uiplace *up, int setup)
{
  int r = 0;

  if (setup) {
    r = init_prop_buf(&up->f, &up->sticky, "sticky", 8, 0, 1, up)
        || init_prop_rect(&up->f, &up->padding, "padding", 1, up)
        || init_prop_rect(&up->f, &up->place, "place", 1, up);
    up->sticky.p.update = up->padding.p.update = up->place.p.update
        = uiplace_prop_update_default;
    if (r) {
      if (up->f.owns_name)
        free(up->f.name);
      free(up);
      return -1;
    }
  }
  up->f_path.name = "path";
  up->f_path.mode = 0600;
  up->f_path.qpath = new_qid(0);
  up->f_path.fs = &path_fs;
  add_file(&up->f, &up->f_path);

  up->place.r[0] = -1;
  up->place.r[1] = -1;
  up->place.r[2] = 1;
  up->place.r[3] = 1;

  up->f.mode = 0500 | P9_DMDIR;
  up->f.qpath = new_qid(FS_UIPLACE);
  up->f.rm = rm_place_data;
  return 0;
}

void
ui_create_place(struct p9_connection *c)
{
  struct uiobj *u;
  struct uiplace *up;

  if (!c->t.pfid->file)
    return;

  u = ((struct uiobj_container *)c->t.pfid->file)->u;
  if (!u->data)
    return;
  up = (struct uiplace *)calloc(1, sizeof(struct uiplace));
  if (!up) {
    P9_SET_STR(c->r.ename, "Cannot allocate memory");
    return;
  }
  up->f.name = strndup(c->t.name, c->t.name_len);
  up->f.owns_name = 1;
  if (!up->f.name || ui_init_place(up, 1)) {
    P9_SET_STR(c->r.ename, "Cannot allocate memory");
    free(up);
    return;
  }
  up->f.rm = rm_place;
  add_file(&((struct uiobj_container *)u->data)->f_items, &up->f);
  up->parent = u->place;
}
