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

static struct uiobj_flags {
  char *s;
  int mask;
} uiobj_flags[] = {
  {"ev_kbd", UI_KBD_EV},
  {"ev_ptr_move", UI_MOVE_PTR_EV},
  {"ev_ptr_updown", UI_UPDOWN_PTR_EV},
  {"ev_ptr_intersect", UI_PTR_INTERSECT_EV},
  {"ev_resize", UI_RESIZE_EV},
  {"exported", UI_EXPORTED},
  {0}
};

extern struct uiobj_maker uitypes[];
static struct arr desktop_place_sticky = {4, 4, "tblr"};
static struct uiplace desktop_place;
struct uiplace *ui_desktop = &desktop_place;
struct uiobj *ui_update_list = 0;
struct uiobj *ui_focused = 0;
struct uiobj *ui_grabbed = 0;
struct uiobj *ui_pointed = 0;

static void ui_set_desktop(struct uiobj *u);
struct uiobj *mk_uiobj(char *name, struct client *c);

struct uiobj_dir {
  struct file f;
};

static void items_create(struct p9_connection *c);

static struct p9_fs items_fs = {
  .create = items_create,
};

static struct p9_fs container_fs = {
  .create = ui_create_place
};

void
ui_enqueue_update(struct uiobj *u)
{
  if (!u || (u->flags & UI_UPD_QUEUED))
    return;
  u->flags |= UI_UPD_QUEUED;
  u->next = ui_update_list;
  ui_update_list = u;
}

void
ui_dequeue_update(struct uiobj *u)
{
  struct uiobj **ppu, *pu;
  if (!(u && (u->flags & UI_UPD_QUEUED)))
    return;
  ppu = &ui_update_list;
  for (pu = *ppu; pu && pu != u; ppu = &pu->next, pu = *ppu) {}
  if (pu)
    *ppu = pu->next;
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
    f = (struct file *)mk_uiobj(name, (struct client *)c);
  else
    f = items_mkdir(name);
  if (!f) {
    P9_SET_STR(c->r.ename, "Cannot allocate memory");
    return;
  }
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
      if (uitypes[i].init(u) == 0) {
        ui_enqueue_update(u);
        good = 1;
      }
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

  u = containerof(fid->file, struct uiobj, f_place);
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
  int *r = u->g.r;

  if (u->bg.i & 0xff000000)
    draw_rect(screen_image, r[0], r[1], r[2], r[3], 0, u->bg.i);
}

void
ui_rm_uiobj(struct file *f)
{
  struct uiobj *u = (struct uiobj *)f;
  struct uiplace *up;

  if (!u)
    return;
  ui_dequeue_update(u);
  up = u->place;
  if (up) {
    up->obj = 0;
    ui_propagate_dirty(up);
  }
  free(u);
}

static struct uiobj_ops empty_obj_ops = {};

static void
flags_open(struct p9_connection *con)
{
  struct p9_fid *fid = con->t.pfid;
  struct uiobj *u = containerof(fid->file, struct uiobj, f_flags);
  struct arr *buf = 0;
  int i, n, off;

  fid->aux = 0;
  fid->rm = rm_fid_aux;

  if (P9_WRITE_MODE(fid->open_mode) && (fid->open_mode & P9_OTRUNC))
    return;
  for (i = 0; uiobj_flags[i].s; ++i)
    if (u->flags & uiobj_flags[i].mask) {
      n = strlen(uiobj_flags[i].s);
      off = (buf) ? buf->used : 0;
      if (arr_memcpy(&buf, 8, -1, n + 1, 0) < 0) {
        P9_SET_STR(con->r.ename, "out of memory");
        return;
      }
      memcpy(buf->b + off, uiobj_flags[i].s, n);
      buf->b[off + n] = '\n';
    }
  fid->aux = buf;
}

static void
flags_read(struct p9_connection *con)
{
  struct arr *buf = con->t.pfid->aux;
  if (buf)
    read_str_fn(con, buf->used, buf->b);
}

static void
flags_write(struct p9_connection *con)
{
  write_buf_fn(con, 16, (struct arr **)&con->t.pfid->aux);
}

static void
flags_clunk(struct p9_connection *con)
{
  struct p9_fid *fid = con->t.pfid;
  struct uiobj *u = containerof(fid->file, struct uiobj, f_flags);
  char *args, *arg;
  int i, flags = u->flags;

  if (!fid->aux)
    return;

  for (i = 0; uiobj_flags[i].s; ++i)
    flags &= ~uiobj_flags[i].mask;
  args = ((struct arr *)fid->aux)->b;
  while ((arg = next_arg(&args)))
    for (i = 0; uiobj_flags[i].s; ++i)
      if (!strcmp(uiobj_flags[i].s, arg))
        flags |= uiobj_flags[i].mask;
  if (!(u->flags & UI_EXPORTED) && (flags & UI_EXPORTED) && !ui_desktop->obj)
    ui_set_desktop(u);
  u->flags = flags;
}

static struct p9_fs flags_fs = {
  .open = flags_open,
  .read = flags_read,
  .write = flags_write,
  .clunk = flags_clunk,
};

void
uiobj_init_flags(struct file *f)
{
  f->name = "flags";
  f->mode = 0600;
  f->qpath = new_qid(FS_UIOBJ_FLAGS);
  f->fs = &flags_fs;
}

struct uiobj *
mk_uiobj(char *name, struct client *client)
{
  int r;
  struct uiobj *u;

  u = (struct uiobj *)calloc(1, sizeof(struct uiobj));
  if (!u)
    return 0;
  u->client = client;
  u->ops = &empty_obj_ops;
  u->f.name = name;
  u->f.mode = 0500 | P9_DMDIR;
  u->f.qpath = new_qid(FS_UIOBJ);
  u->f.rm = ui_rm_uiobj;
  u->flags |= UI_DIRTY;

  r = init_prop_buf(&u->f, &u->type, "type", 0, "", 0, u)
      || init_prop_colour(&u->f, &u->bg, "background", DEF_BG, u)
      || init_prop_rect(&u->f, &u->restraint, "restraint", 1, u)
      || init_prop_rect(&u->f, &u->g, "g", 1, u);

  if (r) {
    rm_file(&u->f);
    free(u);
    u = 0;
  }
  u->bg.p.update = u->restraint.p.update = ui_prop_update_default;

  u->type.p.f.fs = &prop_type_fs;
  u->g.p.f.mode = 0400;

  uiobj_init_flags(&u->f_flags);
  add_file(&u->f, &u->f_flags);

  u->f_place.name = "container";
  u->f_place.mode = 0400;
  u->f_place.qpath = new_qid(0);
  u->f_place.fs = &parent_fs;
  add_file(&u->f, &u->f_place);
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

  if (before_fn(up, aux)) {
    x = (struct uiplace *)uiobj_children(up->obj);
    if (x) {
      do {
        f = 0;
        if (!back && x->obj && before_fn(x, aux))
          f = uiobj_children(x->obj);
        if (f)
          x = (struct uiplace *)f;
        else if (x->f.next) {
          back = 0;
          if (!after_fn(x, aux))
            return;
          x = (struct uiplace *)x->f.next;
        } else if (x->parent) {
          back = 1;
          if (!after_fn(x, aux))
            return;
          x = x->parent;
        }
      } while (x != up);
    }
  }
  after_fn(up, aux);
}

static int
update_place_size(struct uiplace *up, void *aux)
{
  struct uiobj *u;

  if (up && up->obj) {
    u = up->obj;
    if (u->ops->update_size)
      u->ops->update_size(u);
    else
      memcpy(u->reqsize, u->restraint.r, sizeof(u->reqsize));
  }
  return 1;
}

const char *
str_from_rect(int r[4])
{
  static char s[16 * 4 + 5 + 1];
  snprintf(s, sizeof(s), "[%d %d %d %d]", r[0], r[1], r[2], r[3]);
  return s;
}

static int
resize_place(struct uiplace *up, void *aux)
{
  struct uiobj *u = up->obj;
  int i, prev[4], changed = 0;

  if (!u)
    return 1;
  memcpy(prev, u->g.r, sizeof(prev));
  if (u->ops->resize)
    u->ops->resize(u);
  for (i = 0; i < 4 && !changed; ++i)
    changed = (prev[i] != u->g.r[i]);
  if (changed) {
    u->flags |= UI_DIRTY;
    if ((u->flags & UI_SEE_THROUGH) && up->parent && up->parent->obj)
      up->parent->obj->flags |= UI_DIRTY;
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
    const static char *tags[] = {bus_ch_ui, bus_ch_all, 0};
    put_event(u->client->bus, tags, ev);
  }
  memcpy(u->viewport.r, u->g.r, sizeof(u->viewport.r));
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
  int r[4];

  if (!(up && (u = up->obj)))
    return 1;
  if (!(u->flags & UI_DIRTY_VISUAL) && !ctx->dirtyobj)
    return 1;
  if (!ctx->dirtyobj)
    ctx->dirtyobj = u;
  memcpy(up->clip, ctx->clip, sizeof(up->clip));
  ui_intersect_clip(r, ctx->clip, u->viewport.r);
  memcpy(ctx->clip, r, sizeof(ctx->clip));
  set_cliprect(r[0], r[1], r[2], r[3]);
  if (u->ops->draw && r[2] && r[3]) {
    u->ops->draw(u, ctx);
    ctx->dirty = 1;
    add_dirty_rect(r);
  }
  return 1;
}

static int
draw_over_obj(struct uiplace *up, void *aux)
{
  struct uicontext *ctx = (struct uicontext *)aux;
  int *clip = ctx->clip, dirty;
  struct uiobj *u;

  if (!(up && (u = up->obj)))
    return 1;
  dirty = (u->flags & UI_DIRTY_VISUAL) || ctx->dirtyobj;
  if (dirty && u->ops->draw_over && clip[2] && clip[3]) {
    set_cliprect(clip[0], clip[1], clip[2], clip[3]);
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
  return 1;
}

void
ui_propagate_dirty(struct uiplace *up)
{
  ui_enqueue_update(ui_desktop->obj);
  for (; up; up = up->parent)
    if (up->obj)
      up->obj->flags |= UI_DIRTY;
}

void
ui_prop_update_default(struct prop *p)
{
  struct uiobj *u = (struct uiobj *)p->aux;

  if (FSTYPE(*((struct file *)p->aux)) != FS_UIOBJ)
    die("Type error");
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
  ui_desktop->sticky.buf = &desktop_place_sticky;
  f = items_mkdir(name);
  if (f)
    f->fs = &items_fs;
  return f;
}

void
ui_free(void)
{
}

static void
ui_set_desktop(struct uiobj *u)
{
  struct file *f;

  if (u && u != ui_desktop->obj) {
    ui_desktop->obj = u;
    u->place = ui_desktop;
    for (f = uiobj_children(u); f; f = f->next)
      ((struct uiplace *)f)->parent = ui_desktop;
    u->flags |= UI_DIRTY;
    ui_enqueue_update(ui_desktop->obj);
  }
}

int
uifs_update(int force)
{
  static struct uiobj *prev_desk_obj = 0;
  struct uiobj *v, *unext;
  int r[4], flags = 0;

  if (prev_desk_obj != ui_desktop->obj) {
    force = 1;
    prev_desk_obj = ui_desktop->obj;
  }
  cur_time_ms = current_time_ms();
  clean_dirty_rects();
  v = ui_update_list;
  ui_update_list = 0;
  for (; v; v = unext) {
    unext = v->next;
    v->flags &= ~UI_UPD_QUEUED;
    if (v->ops->update)
      v->ops->update(v);
    flags |= (v->flags & UI_DIRTY);
    if (v->flags & UI_DELETED) {
      /* remove uiobj */
    }
  }
  if (force || ((flags & UI_DIRTY) == UI_DIRTY)) {
    walk_ui_tree(ui_desktop, 0, update_place_size, 0);
    r[0] = r[1] = 0;
    r[2] = screen_w;
    r[3] = screen_h;
    ui_place_with_padding(ui_desktop, r);
    walk_ui_tree(ui_desktop, resize_place, 0, 0);
  }
  return (force || flags != 0);
}

int
uifs_redraw(int force)
{
  struct uicontext ctx = {0};
  static struct uiobj *prev_desk_obj = 0;

  if (!ui_desktop->obj && prev_desk_obj) {
    add_dirty_rect2(0, 0, screen_w, screen_h);
    prepare_dirty_rects();
    draw_rect(screen_image, 0, 0, screen_w, screen_h, DEF_BG, DEF_BG);
    return 1;
  }
  prev_desk_obj = ui_desktop->obj;
  if (force)
    ui_desktop->obj->flags |= UI_DIRTY_VISUAL;
  ctx.clip[0] = 0;
  ctx.clip[1] = 0;
  image_get_size(screen_image, &ctx.clip[2], &ctx.clip[3]);
  walk_ui_tree(ui_desktop, draw_obj, draw_over_obj, &ctx);
  prepare_dirty_rects();
  return ctx.dirty;
}
