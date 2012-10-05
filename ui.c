#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <time.h>

#include "config.h"
#include "util.h"
#include "9p.h"
#include "fs.h"
#include "fsutil.h"
#include "fstypes.h"
#include "geom.h"
#include "event.h"
#include "ctl.h"
#include "draw.h"
#include "surface.h"
#include "prop.h"
#include "view.h"
#include "uiobj.h"
#include "ui.h"
#include "client.h"
#include "wm.h"

#define UI_NAME_PREFIX '_'

extern int init_uigrid(struct uiobj *);
extern int init_uiscroll(struct uiobj *);
extern struct uiobj_maker uitypes[];

struct uiobj_dir {
  struct file fs;
  struct client *c;
};

static void items_create(struct p9_connection *c);
static void create_place(struct p9_connection *c);

struct p9_fs items_fs = {
  .create = items_create,
};

struct p9_fs root_items_fs = {
  .create = items_create
};

struct p9_fs container_fs = {
  .create = create_place
};

static void
rm_dir(struct file *dir)
{
  if (dir)
    free(dir);
}

static struct file *
items_mkdir(char *name, struct client *client)
{
  struct uiobj_dir *d;
  d = (struct uiobj_dir *)malloc(sizeof(struct uiobj_dir));
  if (!d)
    return 0;
  memset(d, 0, sizeof(*d));
  d->fs.name = name;
  d->fs.mode = 0700 | P9_DMDIR;
  d->fs.qpath = new_qid(FS_UIDIR);
  d->fs.fs = &items_fs;
  d->fs.rm = rm_dir;
  d->c = client;
  return &d->fs;
}

static void
items_create(struct p9_connection *c)
{
  struct uiobj *u;
  struct file *f;
  struct uiobj_dir *dir;
  char *name;

  if (!(c->t.perm & P9_DMDIR)) {
    P9_SET_STR(c->r.ename, "wrong uiobj create perm");
    return;
  }
  dir = (struct uiobj_dir *)c->t.pfid->file;
  name = strndup(c->t.name, c->t.name_len);
  if (!name) {
    P9_SET_STR(c->r.ename, "Cannot allocate memory");
    return;
  }
  if (name[0] == UI_NAME_PREFIX) {
    u = mk_uiobj(dir->c);
    u->fs.name = name;
    u->fs.owns_name = 1;
    resp_file_create(c, &u->fs);
    add_file(&dir->fs, &u->fs);
  } else {
    f = items_mkdir(name, dir->c);
    f->owns_name = 1;
    add_file(&dir->fs, f);
  }
}

static void
prop_type_open(struct p9_connection *c)
{
  struct prop *p = (struct prop *)c->t.pfid->file;
  struct uiobj *u = (struct uiobj *)p->aux;

  if (u->type.buf && u->type.buf->used && u->type.buf->b[0]
      && (((c->t.pfid->open_mode & 3) == P9_OWRITE)
          || ((c->t.pfid->open_mode & 3) == P9_ORDWR))) {
    c->t.pfid->open_mode = 0;
    P9_SET_STR(c->r.ename, "UI object already has type");
    return;
  }
}

static void
prop_type_clunk(struct p9_connection *c)
{
  struct prop_buf *p = (struct prop_buf *)c->t.pfid->file;
  int i, good = 0;
  struct uiobj *u;

  if (((c->t.pfid->open_mode & 3) != P9_OWRITE)
      && ((c->t.pfid->open_mode & 3) != P9_ORDWR))
    return;
  if (!p->buf)
    return;
  trim_string_right(p->buf->b, " \n\r\t");
  u = (struct uiobj *)p->p.aux;
  for (i = 0; uitypes[i].type && uitypes[i].init; ++i)
    if (!strcmp(uitypes[i].type, p->buf->b))
      if (uitypes[i].init(u) == 0)
        good = 1;
  u->flags |= UI_IS_DIRTY;
  if (!good)
    memset(p->buf->b, 0, p->buf->size);
}

struct p9_fs prop_type_fs = {
  .open = prop_type_open,
  .read = prop_buf_read,
  .write = prop_buf_write,
  .clunk = prop_type_clunk
};

static void
rm_parent_fid(struct p9_fid *fid)
{
  if (fid->aux)
    free(fid->aux);
}

static void
parent_open(struct p9_connection *c)
{
  struct uiobj *u;
  struct client *cl;
  struct p9_fid *fid = c->t.pfid;
  struct arr *buf = 0;
  int n;

  u = containerof(fid->file, struct uiobj, fs_parent);
  cl = (struct client *)u->client;
  n = file_path_len(&u->parent->fs, cl->ui);
  if (arr_memcpy(&buf, n, 0, n, 0))
    die("Cannot allocate memory");
  if (file_path(n, buf->b, &u->parent->fs, cl->ui))
    die("Cannot allocate memory");
  fid->aux = buf;
  c->t.pfid->rm = rm_parent_fid;
}

static void
parent_read(struct p9_connection *c)
{
  struct arr *buf = (struct arr *)c->t.pfid->aux;
  if (buf)
    read_data_fn(c, buf->used, buf->b);
}

struct p9_fs parent_fs = {
  .open = parent_open,
  .read = parent_read
};

void
default_draw_uiobj(struct uiobj *u, struct uicontext *uc)
{
  struct surface *blit = &uc->v->blit;
  unsigned int bg;

  bg = u->bg.i;
  if (bg && 0xff000000)
    fill_rect(blit->img, u->g.r[0], u->g.r[1], u->g.r[2], u->g.r[3], bg);
}

void
ui_rm_uiobj(struct file *f)
{
  struct uiobj *u = (struct uiobj *)f;
  struct uiplace *up;

  if (!u)
    return;
  up = u->parent;
  if (up) {
    up->obj = 0;
    if (up->path.buf)
      up->path.buf->used = 0;
    ui_propagate_dirty(up);
  }
  free(u);
}

struct uiobj *
mk_uiobj(struct client *client)
{
  int r;
  struct uiobj *u;

  u = (struct uiobj *)malloc(sizeof(struct uiobj));
  if (!u)
    return 0;
  memset(u, 0, sizeof(*u));
  u->client = client;
  u->ops = 0;
  u->fs.mode = 0500 | P9_DMDIR;
  u->fs.qpath = new_qid(FS_UIOBJ);
  u->fs.rm = ui_rm_uiobj;
  u->flags |= UI_IS_DIRTY;

  r = init_prop_buf(&u->fs, &u->type, "type", 0, "", 0, u)
      || init_prop_colour(&u->fs, &u->bg, "background", DEFAULT_BG, u)
      || init_prop_int(&u->fs, &u->visible, "visible", 0, u)
      || init_prop_int(&u->fs, &u->drawable, "drawable", 1, u)
      || init_prop_rect(&u->fs, &u->restraint, "restraint", u)
      || init_prop_rect(&u->fs, &u->g, "g", u);

  if (r) {
    rm_file(&u->fs);
    free(u);
    u = 0;
  }

  u->bg.p.update = u->visible.p.update = u->drawable.p.update
    = u->restraint.p.update = ui_prop_update_default;

  u->type.p.fs.fs = &prop_type_fs;
  u->g.p.fs.mode = 0400;

  u->fs_evfilter.name = "evfilter";
  u->fs_evfilter.mode = 0600;
  u->fs_evfilter.qpath = new_qid(0);
  /*u->fs_evfilter.fs = &evfilter_fs;*/
  add_file(&u->fs, &u->fs_evfilter);

  u->fs_parent.name = "container";
  u->fs_parent.mode = 0400;
  u->fs_parent.qpath = new_qid(0);
  u->fs_parent.fs = &parent_fs;
  add_file(&u->fs, &u->fs_parent);

  return u;
}

static void
update_obj_size(struct uiobj *u)
{
  if (!u)
    return;
  log_printf(LOG_UI,
             "upd_obj_size (%p) %s type: %s ops: %p ops->updsize: %p\n",
             u, u->fs.name, u->type.buf->b, u->ops, u->ops->update_size);
  if (u->ops->update_size)
    u->ops->update_size(u);
  else {
    u->reqsize[0] = u->restraint.r[0];
    u->reqsize[1] = u->restraint.r[1];
  }
}

static struct file *
up_children(struct uiplace *up)
{
  struct uiobj *u = up->obj;
  return (u && u->ops->get_children) ? u->ops->get_children(u) : 0;
}

static int
walk_dummy_fn(struct uiplace *up, void *aux)
{
  return 1;
}

void
walk_view_tree(struct uiplace *up,
               int (*before_fn)(struct uiplace *, void *),
               int (*after_fn)(struct uiplace *, void *),
               void *aux)
{
  struct file *f;
  struct uiplace *x, *t;

  if (0) log_printf(LOG_UI, ">> walk_view_tree\n");

  if (!up->obj)
    return;

  before_fn = (before_fn) ? before_fn : walk_dummy_fn;
  after_fn = (after_fn) ? after_fn : walk_dummy_fn;

  x = up;
  x->parent = 0;
  if (0) log_printf(LOG_UI, "  0 x = up: %p '%s'\n", x, x->fs.name);
  do {
    if (0) log_printf(LOG_UI, "  1 x: %p '%s' up: %p\n", x, x->fs.name, up);
    f = 0;
    if (before_fn(x, aux) && x->obj)
      f = up_children(x);
    t = x;
    if (0) log_printf(LOG_UI, "  2 f: %p\n", f);
    if (f) {
      x = (struct uiplace *)f;
      if (0) log_printf(LOG_UI, "  %s->parent <- %p\n", x->fs.name, t);
      x->parent = t;
    } else if (x->fs.next && x != up) {
      if (0) log_printf(LOG_UI, "  !1 x: %p '%s' next: %p parent: %p\n", x,
                        x->fs.name, x->fs.next, x->parent);
      after_fn(x, aux);
      x = (struct uiplace *)x->fs.next;
      x->parent = t->parent;
    } else {
      while (x && x != up && x->parent && !x->parent->fs.next) {
        if (0) log_printf(LOG_UI, "  !2 x: %p '%s'\n", x, x->fs.name);
        after_fn(x, aux);
        x = x->parent;
      }
      if (0) log_printf(LOG_UI, "  !3 x: %p '%s'\n", x, x->fs.name);
      after_fn(x, aux);
      if (x->parent) {
        if (0) log_printf(LOG_UI, "  !4 x: %p '%s'\n", x, x->parent->fs.name);
        after_fn(x->parent, aux);
      }
      if (x != up && x->parent && x->parent != up && x->parent->fs.next
          && x->parent->fs.next != &up->fs) {
        t = x->parent->parent;
        x = (struct uiplace *)x->parent->fs.next;
        x->parent = t;
      } else
        break;
    }
  } while (x && x != up);
  if (0) log_printf(LOG_UI, ">>>> walk_view_tree done\n");
}

static int
update_place_size(struct uiplace *up, void *aux)
{
  if (up && up->obj)
    update_obj_size(up->obj);
  return 1;
}

static int
resize_place(struct uiplace *up, void *aux)
{
  if (up && up->obj && up->obj->ops->resize)
    up->obj->ops->resize(up->obj);
  return 1;
}

static void
intersect_clip(int *c1, int *c2)
{
  int i, r[4], t;

  memcpy(r, c2, sizeof(r));
  for (i = 0; i < 2; ++i) {
    t = c1[i] - r[i];
    if (t > 0) {
      r[i + 2] -= t;
      r[i] = c1[i];
    }
    t = c1[i] + c1[i + 2];
    if (r[i] + r[i + 2] > t)
      r[i + 2] = t - r[i];
  }
  memcpy(c1, r, sizeof(r));
}

static int
update_obj(struct uiplace *up, void *aux)
{
  struct uicontext *ctx = (struct uicontext *)aux;
  struct uiobj *u;

  if (up && up->obj) {
    u = up->obj;
    if (u->ops->update)
      u->ops->update(u, ctx);
  }
  return 1;
}

static int
draw_obj(struct uiplace *up, void *aux)
{
  struct uicontext *ctx = (struct uicontext *)aux;
  struct uiobj *u;
  int *clip = ctx->clip, dirty;

  if (up && up->obj) {
    u = up->obj;
    memcpy(up->clip, clip, sizeof(up->clip));
    intersect_clip(clip, up->clip);
    set_cliprect(clip[0], clip[1], clip[2], clip[3]);
    dirty = (ctx->v->flags & VIEW_IS_DIRTY) || (u->flags & UI_IS_DIRTY);
    if (dirty && u->ops->draw) {
      u->ops->draw(u, ctx);
      ctx->dirty = 1;
    }
  }
  return 1;
}

static int
draw_over_obj(struct uiplace *up, void *aux)
{
  struct uicontext *ctx = (struct uicontext *)aux;
  int *clip = ctx->clip, dirty;
  struct uiobj *u;

  if (up && up->obj) {
    u = up->obj;
    dirty = (ctx->v->flags & VIEW_IS_DIRTY) || (u->flags & UI_IS_DIRTY);
    if (dirty && u->ops->draw_over) {
      u->ops->draw_over(u, ctx);
      ctx->dirty = 1;
    }
    memcpy(clip, up->clip, sizeof(ctx->clip));
    set_cliprect(clip[0], clip[1], clip[2], clip[3]);
    u->flags &= ~UI_IS_DIRTY;
  }
  return 1;
}

void
ui_update_size(struct view *v, struct uiplace *up)
{
}

void
ui_redraw_uiobj(struct uiobj *u)
{
}

static void
rm_place(struct file *f)
{
  struct uiplace *up = (struct uiplace *)f;

  if (!up)
    return;
  if (up->obj) {
    up->obj->parent = 0;
    up->obj = 0;
    ui_propagate_dirty(up);
  }
  if (up->path.buf)
    free(up->path.buf);
  if (up->sticky.buf)
    free(up->sticky.buf);
}

static struct uiobj *
uiplace_container(struct uiplace *up)
{
  if (up->fs.parent && FSTYPE(*up->fs.parent) == FS_UIOBJ)
    return (struct uiobj *)up->fs.parent;
  if (up->fs.parent && (FSTYPE(*up->fs.parent) == FS_UIPLACE_DIR)
      && up->fs.parent->parent && FSTYPE(*up->fs.parent->parent) == FS_UIOBJ)
    return (struct uiobj *)up->fs.parent->parent;
  return 0;
}

static struct view *
uiplace_container_view(struct uiplace *up)
{
  return ((up->fs.parent && (FSTYPE(*up->fs.parent) == FS_VIEW))
          ? (struct view *)up->fs.parent
          : 0);
}

static struct client *
get_place_client(struct uiplace *up)
{
  struct uiobj *u;
  struct view *v;

  if (up->obj)
    return up->obj->client;
  u = uiplace_container(up);
  if (u)
    return u->client;
  v = uiplace_container_view(up);
  if (v)
    return v->c;
  return 0;
}

static int
is_cycled(struct uiplace *up, struct uiobj *u)
{
  struct uiobj *x;
  while (up && (x = uiplace_container(up))) {
    if (x == u)
      return 1;
    up = x->parent;
  }
  return 0;
}

static void
place_uiobj(struct uiplace *up, struct uiobj *u)
{
  if (u) {
    if (is_cycled(up, u))
      return;
    if (u->parent)
      u->parent->obj = 0;
    u->parent = up;
  }
  up->obj = u;
}

static void
unplace_uiobj(struct uiplace *up)
{
  if (!up->obj)
    return;
  up->obj->parent = 0;
  up->obj = 0;
}

void
ui_propagate_dirty(struct uiplace *up)
{
  struct view *v = 0;
  struct uiobj *u;

  log_printf(LOG_UI, ">> ui_propagate_dirty %p\n", up);

  while (up && (u = uiplace_container(up))) {
    u->flags |= UI_IS_DIRTY;
    up = u->parent;
  }
  if (up) {
    v = uiplace_container_view(up);
    if (v && (v->flags & VIEW_IS_VISIBLE))
      v->flags |= VIEW_IS_DIRTY;
  }
}

void
ui_prop_update_default(struct prop *p)
{
  struct uiobj *u = (struct uiobj *)p->aux;

  if (FSTYPE(*((struct file *)p->aux)) != FS_UIOBJ) {
    log_printf(LOG_UI, "ui_default_prop_update type: %d\n",
               FSTYPE(*((struct file *)p->aux)));
    die("Type error");
  }
  u->flags |= UI_IS_DIRTY;
  if (u->parent)
    ui_propagate_dirty(u->parent);
}

void
uiplace_prop_update_default(struct prop *p)
{
  struct uiplace *up = (struct uiplace *)p->aux;
  if (up)
    ui_propagate_dirty(up);
}

static void
upd_path(struct prop *p)
{
  struct prop_buf *pb = (struct prop_buf *)p;
  struct uiplace *up = (struct uiplace *)p->aux;
  struct client *c;
  struct uiobj *prevu;
  struct arr *buf;

  log_printf(LOG_UI, ">> upd_path\n");
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
    uiplace_prop_update_default(p);
}

int
ui_init_place(struct uiplace *up, int setup)
{
  int r;

  r = init_prop_buf(&up->fs, &up->path, "path", 0, "", 0, up);
  if (setup && !r) {
    r = init_prop_buf(&up->fs, &up->sticky, "sticky", 8, 0, 1, up)
        || init_prop_rect(&up->fs, &up->padding, "padding", up)
        || init_prop_rect(&up->fs, &up->place, "place", up);
    up->sticky.p.update = up->padding.p.update = up->place.p.update
        = uiplace_prop_update_default;
  }
  up->place.r[0] = -1;
  up->place.r[1] = -1;
  up->place.r[2] = 1;
  up->place.r[3] = 1;

  if (r) {
    if (up->fs.owns_name)
      free(up->fs.name);
    free(up);
    return -1;
  }
  up->path.p.update = upd_path;

  up->fs.mode = 0500 | P9_DMDIR;
  up->fs.qpath = new_qid(FS_UIPLACE);
  up->fs.rm = rm_place;
  return 0;
}

static void
create_place(struct p9_connection *c)
{
  struct uiobj *u;
  struct uiplace *up;

  log_printf(LOG_UI, ">> create_place\n");

  if (!c->t.pfid->file)
    return;

  u = ((struct uiobj_container *)c->t.pfid->file)->u;
  if (!u->data)
    return;
  up = (struct uiplace *)malloc(sizeof(struct uiplace));
  if (!up) {
    P9_SET_STR(c->r.ename, "Cannot allocate memory");
    return;
  }
  memset(up, 0, sizeof(*up));

  up->fs.name = strndup(c->t.name, c->t.name_len);
  up->fs.owns_name = 1;
  if (!up->fs.name || ui_init_place(up, 1)) {
    P9_SET_STR(c->r.ename, "Cannot allocate memory");
    free(up);
    return;
  }
  add_file(&((struct uiobj_container *)u->data)->fs_items, &up->fs);

  log_printf(LOG_UI, ";; done creating place '%s'\n", up->fs.name);
}

void
ui_init_container_items(struct uiobj_container *c, char *name)
{
  memset(&c->fs_items, 0, sizeof(c->fs_items));
  c->fs_items.name = name;
  c->fs_items.owns_name = 0;
  c->fs_items.mode = 0700 | P9_DMDIR;
  c->fs_items.qpath = new_qid(FS_UIPLACE_DIR);
  c->fs_items.fs = &container_fs;
}

static void
put_uiobj(struct uiobj *u, struct arr *buf, int coord, int r[4], char *opts)
{
  int a, b;

  if (u->reqsize[coord] >= r[coord + 2]) {
    u->g.r[coord] = r[coord];
    u->g.r[coord + 2] = r[coord + 2];
  } else {
    u->g.r[coord] = (r[coord + 2]  - u->reqsize[coord]) >> 1;
    u->g.r[coord + 2] = u->reqsize[coord];
    a = (buf && strnchr(buf->b, buf->used, opts[0]));
    b = (buf && strnchr(buf->b, buf->used, opts[1]));
    if (a && !b)
      u->g.r[coord] = r[coord];
    if (b && !a)
      u->g.r[coord] = r[coord + 2] - u->g.r[coord + 2];
    if (a && b) {
      u->g.r[coord] = r[coord];
      u->g.r[coord + 2] = r[coord + 2];
    }
  }
}

void
ui_place_with_padding(struct uiplace *up, int rect[4])
{
  struct arr *buf;
  struct uiobj *u = up->obj;

  if (!u)
    return;
  log_printf(LOG_UI, ">> ui_place_with_padding '%s'\n", u->fs.name);
  log_printf(LOG_UI, "  rect: [%d %d %d %d]\n", rect[0], rect[1], rect[2],
             rect[3]);
  rect[0] += up->padding.r[0];
  rect[1] += up->padding.r[1];
  rect[2] -= up->padding.r[0] + up->padding.r[2];
  rect[3] -= up->padding.r[1] + up->padding.r[3];
  buf = up->sticky.buf;
  put_uiobj(u, buf, 0, rect, "lr");
  put_uiobj(u, buf, 1, rect, "tb");
  log_printf(LOG_UI, "  uiobj rect: [%d %d %d %d]\n",
             u->g.r[0], u->g.r[1], u->g.r[2], u->g.r[3]);
}

int
ui_init_ui(struct client *c)
{
  c->ui = items_mkdir("ui", c);
  if (!c->ui)
    return -1;
  c->ui->fs = &root_items_fs;
  return 0;
}

int
ui_init_uiplace(struct view *v)
{
  struct uiplace *up;

  up = (struct uiplace *)malloc(sizeof(struct uiplace));
  memset(up, 0, sizeof(*up));
  if (!up || ui_init_place(up, 1))
    return -1;
  v->uiplace = &up->fs;
  return 0;
}

void
ui_free()
{
}

void
ui_update_view(struct view *v)
{
  struct uiplace *up = (struct uiplace *)v->uiplace;

  log_printf(LOG_UI, ">> ui_update_view '%s'\n", v->fs.name);

  if (!(up && up->obj))
    return;

  walk_view_tree((struct uiplace *)v->uiplace, 0, update_place_size, 0);
  wm_view_size_request(v);
  ui_place_with_padding(up, v->g.r);
  walk_view_tree(up, resize_place, 0, 0);
}

int
ui_redraw_view(struct view *v)
{
  struct uiplace *up = (struct uiplace *)v->uiplace;
  struct uicontext ctx = {0};

  if (!(up && up->obj))
    return 0;

  ctx.v = v;
  memcpy(ctx.clip, v->g.r, sizeof(ctx.clip));
  walk_view_tree(up, 0, update_obj, &ctx);
  walk_view_tree(up, draw_obj, draw_over_obj, &ctx);
  return ctx.dirty;
}

int
ui_update()
{
  return 0;
}

