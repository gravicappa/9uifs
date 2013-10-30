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
#include "bus.h"
#include "ctl.h"
#include "backend.h"
#include "surface.h"
#include "prop.h"
#include "uiobj.h"
#include "ui.h"
#include "uiplace.h"
#include "client.h"
#include "dirty.h"

#define UI_NAME_PREFIX '_'

static struct uiplace desktop_place;
struct uiplace *ui_desktop = &desktop_place;
struct uiobj *ui_update_list = 0;
struct uiobj *ui_focused = 0;
struct uiobj *ui_grabbed = 0;
struct uiobj *ui_pointed = 0;

int init_uigrid(struct uiobj *);
int init_uiscroll(struct uiobj *);
extern struct uiobj_maker uitypes[];

struct uiobj_dir {
  struct file f;
};

static void items_create(struct p9_connection *c);

static struct p9_fs items_fs = {
  .create = items_create,
};

static struct p9_fs root_items_fs = {
  .create = items_create
};

static struct p9_fs container_fs = {
  .create = ui_create_place
};

void
ui_enqueue_update(struct uiobj *u)
{
  u->next = ui_update_list;
  ui_update_list = u;
}

static void
rm_dir(struct file *dir)
{
  if (dir)
    free(dir);
}

static struct file *
items_mkdir(const char *name)
{
  struct uiobj_dir *d;
  d = (struct uiobj_dir *)calloc(1, sizeof(struct uiobj_dir));
  if (!d)
    return 0;
  d->f.name = (char *)name;
  d->f.mode = 0700 | P9_DMDIR;
  d->f.qpath = new_qid(FS_UIDIR);
  d->f.fs = &items_fs;
  d->f.rm = rm_dir;
  return (struct file *)d;
}

static void
items_create(struct p9_connection *c)
{
  struct file *f;
  struct uiobj_dir *dir;
  char *name;

  if (!(c->t.perm & P9_DMDIR)) {
    P9_SET_STR(c->r.ename, "Wrong uiobj create permissions");
    return;
  }
  dir = (struct uiobj_dir *)c->t.pfid->file;
  name = strndup(c->t.name, c->t.name_len);
  if (!name) {
    P9_SET_STR(c->r.ename, "Cannot allocate memory");
    return;
  }
  if (name[0] == UI_NAME_PREFIX)
    f = (struct file *)mk_uiobj((struct client *)c);
  else
    f = items_mkdir(name);
  if (!f) {
    P9_SET_STR(c->r.ename, "Cannot allocate memory");
    return;
  }
  f->name = name;
  f->owns_name = 1;
  resp_file_create(c, f);
  add_file(&dir->f, f);
}

static void
prop_type_open(struct p9_connection *c)
{
  struct prop *p = (struct prop *)c->t.pfid->file;
  struct uiobj *u = (struct uiobj *)p->aux;

  if (u->type.buf && u->type.buf->used && u->type.buf->b[0]
      && P9_WRITE_MODE(c->t.pfid->open_mode)) {
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
parent_open(struct p9_connection *c)
{
  struct uiobj *u;
  struct client *cl;
  struct p9_fid *fid = c->t.pfid;
  struct arr *buf = 0;
  int n;

  u = containerof(fid->file, struct uiobj, f_parent);
  if (!u->place)
    return;
  cl = (struct client *)u->client;
  n = file_path_len(&u->place->f, cl->ui);
  if (arr_memcpy(&buf, n, 0, n, 0))
    die("Cannot allocate memory [1]");
  file_path(n, buf->b, &u->place->f, cl->ui);
  fid->aux = buf;
  c->t.pfid->rm = rm_fid_aux;
}

static void
parent_read(struct p9_connection *c)
{
  struct arr *buf = (struct arr *)c->t.pfid->aux;
  if (buf)
    read_data_fn(c, buf->used, buf->b);
}

static struct p9_fs parent_fs = {
  .open = parent_open,
  .read = parent_read
};

void
default_draw_uiobj(struct uiobj *u, struct uicontext *uc)
{
  unsigned int bg;

  bg = u->bg.i;
  if (bg & 0xff000000)
    draw_rect(screen_image, u->g.r[0], u->g.r[1], u->g.r[2], u->g.r[3], 0,
              bg);
}

void
ui_rm_uiobj(struct file *f)
{
  struct uiobj *u = (struct uiobj *)f;
  struct uiplace *up;

  if (!u)
    return;
  up = u->place;
  if (up) {
    up->obj = 0;
    ui_propagate_dirty(up);
  }
  free(u);
}

static void
ui_prop_drawable_upd(struct prop *p)
{
  struct uiobj *u = (struct uiobj *)p->aux;
  u->flags |= UI_DIRTY;
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
      || init_prop_colour(&u->f, &u->bg, "background", DEF_BG, u)
      || init_prop_int(&u->f, &u->visible, "visible", 0, u)
      || init_prop_int(&u->f, &u->drawable, "drawable", 1, u)
      || init_prop_rect(&u->f, &u->restraint, "restraint", 1, u)
      || init_prop_rect(&u->f, &u->g, "g", 1, u);

  if (r) {
    rm_file(&u->f);
    free(u);
    u = 0;
  }

  u->bg.p.update = u->visible.p.update = u->restraint.p.update
        = ui_prop_update_default;
  u->drawable.p.update = ui_prop_drawable_upd;

  u->type.p.f.fs = &prop_type_fs;
  u->g.p.f.mode = 0400;

  ui_init_evfilter(&u->f_evfilter);
  add_file(&u->f, &u->f_evfilter);

  u->f_parent.name = "container";
  u->f_parent.mode = 0400;
  u->f_parent.qpath = new_qid(0);
  u->f_parent.fs = &parent_fs;
  add_file(&u->f, &u->f_parent);

  return u;
}

struct file *
uiobj_children(struct uiobj *u)
{
  return (u && u->ops->get_children) ? u->ops->get_children(u) : 0;
}

static int
walk_empty_fn(struct uiplace *up, void *aux)
{
  return 1;
}

void
walk_ui_tree(struct uiplace *up,
             int (*before_fn)(struct uiplace *, void *),
             int (*after_fn)(struct uiplace *, void *),
             void *aux)
{
  struct file *f;
  struct uiplace *x;
  int back = 0;

  if (!(up && up->obj))
    return;

  before_fn = (before_fn) ? before_fn : walk_empty_fn;
  after_fn = (after_fn) ? after_fn : walk_empty_fn;

  x = up;
  do {
    f = 0;
    if (!back && x->obj && before_fn(x, aux))
      f = uiobj_children(x->obj);
    if (f)
      x = (struct uiplace *)f;
    else if (x->f.next) {
      back = 0;
      if (!after_fn(x, aux))
        goto end;
      x = (struct uiplace *)x->f.next;
    } else {
      back = 1;
      if (!after_fn(x, aux))
        goto end;
      x = x->parent;
    }
  } while (x != up);
  after_fn(x, aux);
end:
  ;
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
  int i, prev[4], changed = 0;

  if (u) {
    for (i = 0; i < 4; ++i)
      prev[i] = u->g.r[i];
    if (u->ops->resize)
      u->ops->resize(u);
    for (i = 0; i < 4 && !changed; ++i)
      changed = (prev[i] != u->g.r[i]);

    if (changed) {
      u->flags |= UI_DIRTY;
      if ((u->flags & UI_SEE_THROUGH) && up->parent && up->parent->obj)
        up->parent->obj->flags |= UI_DIRTY;
      add_dirty_rect(u->g.r);
    }
    if (changed && (u->flags & UI_RESIZE_EV)) {
      struct ev_arg ev[] = {
        {ev_str, {.s = "resize"}},
        {ev_uiobj, {.o = u}},
        {ev_int, {.i = u->g.r[0]}},
        {ev_int, {.i = u->g.r[1]}},
        {ev_int, {.i = u->g.r[2]}},
        {ev_int, {.i = u->g.r[3]}},
        {0}
      };
      const static char *tags[] = {bus_ch_all, bus_ch_ui, 0};
      put_event(u->client->bus, tags, ev);
    }
    memcpy(u->viewport.r, u->g.r, sizeof(u->viewport.r));
  }
  return 1;
}

void
ui_intersect_clip(int *r, int *c1, int *c2)
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
draw_obj(struct uiplace *up, void *aux)
{
  struct uicontext *ctx = (struct uicontext *)aux;
  struct uiobj *u;
  int *clip = ctx->clip, r[4], i, *drect;

  if (up && up->obj) {
    u = up->obj;
    if (!(u->flags & UI_DIRTY) || ctx->dirtyobj)
      return 1;
    if (!ctx->dirtyobj)
      ctx->dirtyobj = u;
    memcpy(up->clip, clip, sizeof(up->clip));
    ui_intersect_clip(r, clip, u->viewport.r);
    memcpy(clip, r, sizeof(ctx->clip));
    if (u->ops->draw) {
      for (drect = dirty_rects, i = 0; i < ndirty_rects; ++i, drect += 4) {
        ui_intersect_clip(r, clip, drect);
        if (r[2] && r[3]) {
          set_cliprect(r[0], r[1], r[2], r[3]);
          u->ops->draw(u, ctx);
          ctx->dirty = 1;
        }
      }
    }
    set_cliprect(clip[0], clip[1], clip[2], clip[3]);
  }
  return 1;
}

static int
draw_over_obj(struct uiplace *up, void *aux)
{
  struct uicontext *ctx = (struct uicontext *)aux;
  int *clip = ctx->clip, dirty, i, *drect, r[4];
  struct uiobj *u;

  if (up && up->obj) {
    u = up->obj;
    dirty = (u->flags & UI_DIRTY) || ctx->dirtyobj;
    if (dirty && u->ops->draw_over) {
      for (drect = dirty_rects, i = 0; i < ndirty_rects; ++i, drect += 4) {
        ui_intersect_clip(r, clip, drect);
        if (r[2] && r[3]) {
          set_cliprect(r[0], r[1], r[2], r[3]);
          u->ops->draw_over(u, ctx);
          ctx->dirty = 1;
        }
      }
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
ui_propagate_dirty(struct uiplace *up)
{
  struct uiobj *u;

  while (up && (u = uiplace_container(up))) {
    u->flags |= UI_DIRTY;
    up = u->place;
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
  if (u->place)
    ui_propagate_dirty(u->place);
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
ev_uiobj(char *buf, struct ev_arg *ev)
{
  struct uiobj *u = ev->x.o;
  if (!buf)
    return file_path_len((struct file *)u, u->client->ui) - 1;
  return file_path(ev->len + 1, buf, (struct file *)u, u->client->ui) - 1;
}

struct file *
mk_ui(const char *name)
{
  struct file *f;
  f = items_mkdir(name);
  if (f)
    f->fs = &root_items_fs;
  return f;
}

void
ui_free(void)
{
}

static void
ui_set_desktop(struct uiobj *u)
{
  static struct uiobj *prev;
  struct file *f;
  ui_desktop->obj = u;
  if (u && u != prev) {
    u->place = ui_desktop;
    for (f = uiobj_children(u); f; f = f->next)
      ((struct uiobj *)f)->place->parent = ui_desktop;
    prev = u;
  }
}

void
uifs_update(void)
{
  struct uiobj *v, *unext;

  cur_time_ms = current_time_ms();
  ui_set_desktop(ui_desktop->obj);
  v = ui_update_list;
  ui_update_list = 0;
  for (; v; v = unext) {
    unext = v->next;
    if (v->ops->update)
      v->ops->update(v);
  }
  if (ui_desktop->obj) {
    walk_ui_tree(ui_desktop, 0, update_place_size, 0);
    walk_ui_tree(ui_desktop, resize_place, 0, 0);
  }
}

int
uifs_redraw(int force)
{
  struct uicontext ctx = {0};
  ui_set_desktop(ui_desktop->obj);

  if (!ui_desktop->obj)
    return 0;

  if (force)
    ui_desktop->obj->flags |= UI_DIRTY;
  ctx.clip[0] = 0;
  ctx.clip[1] = 0;
  image_get_size(screen_image, &ctx.clip[2], &ctx.clip[3]);
  walk_ui_tree(ui_desktop, draw_obj, draw_over_obj, &ctx);
  return ctx.dirty;
}
