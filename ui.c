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
  struct file f;
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
  d = (struct uiobj_dir *)calloc(1, sizeof(struct uiobj_dir));
  if (!d)
    return 0;
  d->f.name = name;
  d->f.mode = 0700 | P9_DMDIR;
  d->f.qpath = new_qid(FS_UIDIR);
  d->f.fs = &items_fs;
  d->f.rm = rm_dir;
  d->c = client;
  return (struct file *)d;
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
    u->f.name = name;
    u->f.owns_name = 1;
    resp_file_create(c, &u->f);
    add_file(&dir->f, &u->f);
  } else {
    f = items_mkdir(name, dir->c);
    f->owns_name = 1;
    add_file(&dir->f, f);
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
  u->flags |= UI_DIRTY;
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

  u = containerof(fid->file, struct uiobj, f_parent);
  cl = (struct client *)u->client;
  n = file_path_len(&u->parent->f, cl->ui);
  if (arr_memcpy(&buf, n, 0, n, 0))
    die("Cannot allocate memory [1]");
  file_path(n, buf->b, &u->parent->f, cl->ui);
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

  u = (struct uiobj *)calloc(1, sizeof(struct uiobj));
  if (!u)
    return 0;
  u->client = client;
  u->ops = 0;
  u->f.mode = 0500 | P9_DMDIR;
  u->f.qpath = new_qid(FS_UIOBJ);
  u->f.rm = ui_rm_uiobj;
  u->flags |= UI_DIRTY;

  r = init_prop_buf(&u->f, &u->type, "type", 0, "", 0, u)
      || init_prop_colour(&u->f, &u->bg, "background", DEFAULT_BG, u)
      || init_prop_int(&u->f, &u->visible, "visible", 0, u)
      || init_prop_int(&u->f, &u->drawable, "drawable", 1, u)
      || init_prop_rect(&u->f, &u->restraint, "restraint", 1, u)
      || init_prop_rect(&u->f, &u->g, "g", 1, u);

  if (r) {
    rm_file(&u->f);
    free(u);
    u = 0;
  }

  u->bg.p.update = u->visible.p.update = u->drawable.p.update
    = u->restraint.p.update = ui_prop_update_default;

  u->type.p.f.fs = &prop_type_fs;
  u->g.p.f.mode = 0400;

  u->f_evfilter.name = "evfilter";
  u->f_evfilter.mode = 0600;
  u->f_evfilter.qpath = new_qid(0);
  /*u->f_evfilter.fs = &evfilter_fs;*/
  add_file(&u->f, &u->f_evfilter);

  u->f_parent.name = "container";
  u->f_parent.mode = 0400;
  u->f_parent.qpath = new_qid(0);
  u->f_parent.fs = &parent_fs;
  add_file(&u->f, &u->f_parent);

  return u;
}

static void
update_obj_size(struct uiobj *u)
{
  if (!u)
    return;
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
#if 1
  return (u && u->ops->get_children) ? u->ops->get_children(u) : 0;
#else
  return (u && u->ops->get_children)
         ? ((struct uiobj_container *)u->data)->f_items.child
         : 0;
#endif
}

static int
walk_dummy_fn(struct uiplace *up, void *aux)
{
  return 1;
}

void
ui_walk_view_tree(struct uiplace *up,
                  int (*before_fn)(struct uiplace *, void *),
                  int (*after_fn)(struct uiplace *, void *),
                  void *aux)
{
  struct file *f;
  struct uiplace *x, *t;

#if 0
  log_printf(LOG_UI, ">> ui_walk_view_tree\n");
#endif

  if (!up->obj)
    return;

  before_fn = (before_fn) ? before_fn : walk_dummy_fn;
  after_fn = (after_fn) ? after_fn : walk_dummy_fn;

  x = up;
  x->parent = 0;
#if 0
  log_printf(LOG_UI, "  0 x = up: %p '%s'\n", x, x->f.name);
#endif
  do {
#if 0
    log_printf(LOG_UI, "  1 x: %p '%s' up: %p\n", x, x->f.name, up);
#endif
    f = 0;
    if (x->obj && before_fn(x, aux))
      f = up_children(x);
    t = x;
#if 0
    log_printf(LOG_UI, "  2 f: %p\n", f);
#endif
    if (f) {
      x = (struct uiplace *)f;
#if 0
      log_printf(LOG_UI, "  %s->parent <- %p\n", x->f.name, t);
#endif
      x->parent = t;
    } else if (x->f.next && x != up) {
#if 0
      log_printf(LOG_UI, "  !1 x: %p '%s' next: %p parent: %p\n", x,
                 x->f.name, x->f.next, x->parent);
#endif
      if (!after_fn(x, aux))
        return;
      x = (struct uiplace *)x->f.next;
      x->parent = t->parent;
    } else {
      while (x && x != up && x->parent && !x->parent->f.next) {
#if 0
        if (0)
          log_printf(LOG_UI, "  !2 x: %p '%s'\n", x, x->f.name);
#endif
        if (!after_fn(x, aux))
          return;
        x = x->parent;
      }
#if 0
      log_printf(LOG_UI, "  !3 x: %p '%s'\n", x, x->f.name);
#endif
      if (!after_fn(x, aux))
        return;
      if (x->parent) {
#if 0
        if (0)
          log_printf(LOG_UI, "  !4 x: %p '%s'\n", x, x->parent->f.name);
#endif
        if (!after_fn(x->parent, aux))
          return;
      }
      if (x != up && x->parent && x->parent != up && x->parent->f.next
          && x->parent->f.next != &up->f) {
        t = x->parent->parent;
        x = (struct uiplace *)x->parent->f.next;
        x->parent = t;
      } else
        break;
    }
  } while (x && x != up);
#if 0
  log_printf(LOG_UI, ">>>> ui_walk_view_tree done\n");
#endif
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
  struct uiobj *u = up->obj;

  if (u) {
    if (u->ops->resize)
      u->ops->resize(u);
    memcpy(u->viewport.r, u->g.r, sizeof(u->viewport.r));
  }
  return 1;
}

static void
intersect_clip(int *r, int *c1, int *c2)
{
  int i, t;

  memcpy(r, c2, sizeof(int[4]));
  for (i = 0; i < 2; ++i) {
    t = c1[i] - r[i];
    if (t > 0) {
      r[i + 2] -= (r[i + 2] > t) ? t : r[i + 2];
      r[i] = c1[i];
    }
    t = c1[i] + c1[i + 2];
    if (r[i] + r[i + 2] > t)
      r[i + 2] = t - r[i];
  }
  if (0)
    log_printf(LOG_UI, "[%d %d %d %d] ^ [%d %d %d %d] = [%d %d %d %d]\n",
               c1[0], c1[1], c1[2], c1[3],
               c2[0], c2[1], c2[2], c2[3], r[0], r[1], r[2], r[3]);
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
  int *clip = ctx->clip, dirty, r[4];

  if (up && up->obj) {
    u = up->obj;
    dirty = ((ctx->v->flags & VIEW_DIRTY) || (u->flags & UI_DIRTY)
            || ctx->dirtyobj);
    if (!dirty)
      return 1;
    if (!ctx->dirtyobj)
      ctx->dirtyobj = u;
    memcpy(up->clip, clip, sizeof(up->clip));
    intersect_clip(r, clip, u->viewport.r);
    memcpy(clip, r, sizeof(ctx->clip));
    set_cliprect(clip[0], clip[1], clip[2], clip[3]);
    if (u->ops->draw) {
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
    dirty = ((ctx->v->flags & VIEW_DIRTY) || (u->flags & UI_DIRTY)
            || ctx->dirtyobj);
    if (dirty && u->ops->draw_over) {
      u->ops->draw_over(u, ctx);
      ctx->dirty = 1;
    }
    if (ctx->dirtyobj == u)
      ctx->dirtyobj = 0;
    if (dirty) {
      memcpy(clip, up->clip, sizeof(ctx->clip));
      set_cliprect(clip[0], clip[1], clip[2], clip[3]);
    }
    u->flags &= ~UI_DIRTY;
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
  if (up->f.parent && FSTYPE(*up->f.parent) == FS_UIOBJ)
    return (struct uiobj *)up->f.parent;
  if (up->f.parent && (FSTYPE(*up->f.parent) == FS_UIPLACE_DIR)
      && up->f.parent->parent && FSTYPE(*up->f.parent->parent) == FS_UIOBJ)
    return (struct uiobj *)up->f.parent->parent;
  return 0;
}

static struct view *
uiplace_container_view(struct uiplace *up)
{
  return ((up->f.parent && (FSTYPE(*up->f.parent) == FS_VIEW))
          ? (struct view *)up->f.parent
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

  while (up && (u = uiplace_container(up))) {
    u->flags |= UI_DIRTY;
    up = u->parent;
  }
  if (up) {
    v = uiplace_container_view(up);
    if (v && (v->flags & VIEW_VISIBLE))
      v->flags |= VIEW_DIRTY;
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
  u->flags |= UI_DIRTY;
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

  c = get_place_client(up);
  prevu = up->obj;
  if (up->obj)
    unplace_uiobj(up);
  if (!c)
    return;
  up->obj = 0;
  buf = pb->buf;
  if (buf && buf->used > 0) {
    for (; buf->b[buf->used - 1] <= ' '; --buf->used) {}
    place_uiobj(up, (struct uiobj *)find_file(c->ui, buf->used, buf->b));
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

  r = init_prop_buf(&up->f, &up->path, "path", 0, "", 0, up);
  if (setup && !r) {
    r = init_prop_buf(&up->f, &up->sticky, "sticky", 8, 0, 1, up)
        || init_prop_rect(&up->f, &up->padding, "padding", 1, up)
        || init_prop_rect(&up->f, &up->place, "place", 1, up);
    up->sticky.p.update = up->padding.p.update = up->place.p.update
        = uiplace_prop_update_default;
  }
  up->place.r[0] = -1;
  up->place.r[1] = -1;
  up->place.r[2] = 1;
  up->place.r[3] = 1;

  if (r) {
    if (up->f.owns_name)
      free(up->f.name);
    free(up);
    return -1;
  }
  up->path.p.update = upd_path;

  up->f.mode = 0500 | P9_DMDIR;
  up->f.qpath = new_qid(FS_UIPLACE);
  up->f.rm = rm_place;
  return 0;
}

static void
create_place(struct p9_connection *c)
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
  add_file(&((struct uiobj_container *)u->data)->f_items, &up->f);
}

void
ui_init_container_items(struct uiobj_container *c, char *name)
{
  memset(&c->f_items, 0, sizeof(c->f_items));
  c->f_items.name = name;
  c->f_items.owns_name = 0;
  c->f_items.mode = 0700 | P9_DMDIR;
  c->f_items.qpath = new_qid(FS_UIPLACE_DIR);
  c->f_items.fs = &container_fs;
}

static void
put_uiobj(struct uiobj *u, struct arr *buf, int coord, int r[4], char *opts)
{
  int flag = 0;

  if (u->reqsize[coord] >= r[coord + 2]) {
    u->g.r[coord] = r[coord];
    u->g.r[coord + 2] = r[coord + 2];
  } else {
    if (buf) {
      flag = (strnchr(buf->b, buf->used, opts[0]) != 0);
      flag |= (strnchr(buf->b, buf->used, opts[1]) != 0) << 1;
    }
    u->g.r[coord + 2] = u->reqsize[coord];
    switch (flag) {
    case 0:
      u->g.r[coord] = r[coord] + ((r[coord + 2] - u->reqsize[coord]) >> 1);
      break;
    case 1:
      u->g.r[coord] = r[coord];
      break;
    case 2:
      u->g.r[coord] = r[coord] + (r[coord + 2] - u->g.r[coord + 2]);
      break;
    case 3:
      u->g.r[coord] = r[coord];
      u->g.r[coord + 2] = r[coord + 2];
    }
  }
}

void
ui_place_with_padding(struct uiplace *up, int rect[4])
{
  struct uiobj *u = up->obj;

  if (!u)
    return;
  rect[0] += up->padding.r[0];
  rect[1] += up->padding.r[1];
  rect[2] -= up->padding.r[0] + up->padding.r[2];
  rect[3] -= up->padding.r[1] + up->padding.r[3];
  put_uiobj(u, up->sticky.buf, 0, rect, "lr");
  put_uiobj(u, up->sticky.buf, 1, rect, "tb");
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

  up = (struct uiplace *)calloc(1, sizeof(struct uiplace));
  if (!up || ui_init_place(up, 1))
    return -1;
  v->uiplace = (struct file *)up;
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

  if (!(up && up->obj))
    return;

  ui_walk_view_tree((struct uiplace *)v->uiplace, 0, update_place_size, 0);
  wm_view_size_request(v);
  ui_place_with_padding(up, v->g.r);
  ui_walk_view_tree(up, resize_place, 0, 0);
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
  ui_walk_view_tree(up, 0, update_obj, &ctx);
  ui_walk_view_tree(up, draw_obj, draw_over_obj, &ctx);
  return ctx.dirty;
}

int
ui_update()
{
  return 0;
}
