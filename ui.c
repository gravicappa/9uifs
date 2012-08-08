#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>

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

struct uiobj_maker {
  char *type;
  int (*init)(struct uiobj *);
} uitypes[] = {
  {"grid", init_uigrid},
  {"scroll", init_uiscroll},
  /*
  {"button", init_uibutton},
  {"label", init_uilabel},
  {"entry", init_uientry},
  {"blit", init_uientry},
  */
  {0, 0}
};

struct uiobj_dir {
  struct file fs;
  struct client *c;
};

extern struct p9_fs places_fs;

static void items_create(struct p9_connection *c);
static void create_place(struct p9_connection *c);

static struct arr *stack = 0;
static int prevframecnt = -1;

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

  if (((c->t.pfid->open_mode & 3) != P9_OWRITE)
      && ((c->t.pfid->open_mode & 3) != P9_ORDWR))
    return;
  if (!p->buf)
    return;
  trim_string_right(p->buf->b, " \n\r\t");
  for (i = 0; uitypes[i].type && uitypes[i].init; ++i)
    if (!strcmp(uitypes[i].type, p->buf->b))
      if (uitypes[i].init((struct uiobj *)p->p.aux) == 0)
        good = 1;
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
draw_uiobj(struct uiobj *u, struct view *v)
{
  unsigned int c;

  c = u->bg.i;
  if (c && 0xff000000)
    fill_rect(v->blit.img, u->g.r[0], u->g.r[1], u->g.r[2], u->g.r[3], c);
}

void
ui_rm_uiobj(struct file *f)
{
  struct uiobj *u = (struct uiobj *)f;
  if (!u)
    return;
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
  u->draw = draw_uiobj;
  u->fs.mode = 0500 | P9_DMDIR;
  u->fs.qpath = new_qid(FS_UIOBJ);
  u->fs.rm = ui_rm_uiobj;

  /* TODO: use bg from some kind of style db */
  r = init_prop_buf(&u->fs, &u->type, "type", 0, "", 0, u)
      | init_prop_colour(&u->fs, &u->bg, "background", 0, u)
      | init_prop_int(&u->fs, &u->visible, "visible", 0, u)
      | init_prop_int(&u->fs, &u->drawable, "drawable", 1, u)
      | init_prop_rect(&u->fs, &u->restraint, "restraint", u)
      | init_prop_rect(&u->fs, &u->g, "g", u);

  if (r) {
    rm_file(&u->fs);
    free(u);
    u = 0;
  }

  u->type.p.fs.fs = &prop_type_fs;
  u->g.p.fs.mode = 0400;

  u->fs_evfilter.name = "evfilter";
  u->fs_evfilter.mode = 0600;
  u->fs_evfilter.qpath = new_qid(0);
  /*u->fs_evfilter.fs = &evfilter_fs;*/
  add_file(&u->fs, &u->fs_evfilter);

  u->fs_places.name = "places";
  u->fs_places.mode = 0400;
  u->fs_places.qpath = new_qid(0);
  u->fs_places.fs = &places_fs;
  add_file(&u->fs, &u->fs_places);

  return u;
}

static void
update_obj_size(struct uiobj *u)
{
  if (!u)
    return;
  if (u->update_size)
    u->update_size(u);
  u->reqsize[0] = u->restraint.r[0];
  u->reqsize[1] = u->restraint.r[1];
}

static struct file *
up_children(struct uiplace *up)
{
  if (!(up->obj && (up->obj->flags & UI_IS_CONTAINER) && up->obj->data))
    return 0;
  return ((struct uiobj_container *)up->obj->data)->fs_items.child;
}

static void
walk_dummy_fn(struct uiplace *up, struct view *v, void *aux)
{
}

static void
walk_view_tree(struct uiplace *up, struct view *v, void *aux,
               void (*before_fn)(struct uiplace *, struct view *, void *),
               void (*after_fn)(struct uiplace *, struct view *, void *))
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
    before_fn(x, v, aux);
    if (0) log_printf(LOG_UI, "  1 x: %p '%s' up: %p frame:%d/%d\n", x,
               x->fs.name, up, (x->obj) ? x->obj->frame : -1, framecnt);
    f = 0;
    if (x->obj && x->obj->frame1 != framecnt) {
      f = up_children(x);
      x->obj->frame1 = framecnt;
    }
    t = x;
    if (0) log_printf(LOG_UI, "  2 f: %p\n", f);
    if (f) {
      x = (struct uiplace *)f;
      x->parent = t;
    } else if (x->fs.next && x != up) {
      if (0) log_printf(LOG_UI, "  !1 x: %p '%s' next: %p parent: %p\n", x,
                        x->fs.name, x->fs.next, x->parent);
      after_fn(x, v, aux);
      x = (struct uiplace *)x->fs.next;
      x->parent = t->parent;
    } else {
      while (x && x != up && x->parent && !x->parent->fs.next) {
        if (0) log_printf(LOG_UI, "  !2 x: %p '%s'\n", x, x->fs.name);
        after_fn(x, v, aux);
        x = x->parent;
      }
      if (0) log_printf(LOG_UI, "  !3 x: %p '%s'\n", x, x->fs.name);
      after_fn(x, v, aux);
      if (x->parent) {
        if (0) log_printf(LOG_UI, "  !4 x: %p '%s'\n", x, x->parent->fs.name);
        after_fn(x->parent, v, aux);
      }
      if (x != up && x->parent && x->parent != up && x->parent->fs.next
          && x->parent->fs.next != &up->fs)
        x = (struct uiplace *)x->parent->fs.next;
      else
        break;
    }
  } while (x && x != up);
  if (0) log_printf(LOG_UI, ">>>> walk_view_tree done\n");
}

static void
update_place_size(struct uiplace *up, struct view *v, void *aux)
{
  if (up && up->obj && up->obj->frame != framecnt) {
    update_obj_size(up->obj);
    up->obj->frame = framecnt;
  }
}

static void
resize_place(struct uiplace *up, struct view *v, void *aux)
{
  if (up && up->obj && up->obj->resize && up->obj->frame != framecnt) {
    up->obj->resize(up->obj);
    up->obj->frame = framecnt;
  }
}

static void
draw_obj(struct uiplace *up, struct view *v, void *aux)
{
  if (up && up->obj && up->obj->draw && up->obj->frame != framecnt) {
    up->obj->frame = framecnt;
    up->obj->draw(up->obj, v);
  }
}

void
ui_update_size(struct view *v, struct uiplace *up)
{
}

void
redraw_uiobj(struct uiobj *u)
{
  ;
}

static void
rm_place(struct file *f)
{
  struct uiplace *up = (struct uiplace *)f;
  if (!up)
    return;
  if (up->path.buf)
    free(up->path.buf);
  if (up->sticky.buf)
    free(up->sticky.buf);
}

static struct uiobj *
uiplace_container(struct uiplace *up)
{
  return ((up->fs.parent && (FSTYPE(*up->fs.parent) == FS_UIPLACE_DIR)
           && up->fs.parent->parent)
          ? (struct uiobj *)up->fs.parent->parent : 0);
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

static void
place_uiobj(struct uiplace *up, struct uiobj *u)
{
  struct uiobj_parent *p;

  if (u) {
    p = (struct uiobj_parent *)malloc(sizeof(struct uiobj_parent));
    if (!p)
      die("Cannot allocate memory");
    p->place = up;
    p->prev = 0;
    p->next = u->parents;
    if (u->parents)
      u->parents->prev = p;
    u->parents = p;
  }
  up->obj = u;
}

static void
unplace_uiobj(struct uiplace *up)
{
  struct uiobj_parent *p;

  if (!up->obj)
    return;
  log_printf(LOG_UI, ">> unplace_uiobj u: %p\n", up->obj);
  p = up->obj->parents;
  for (; p && p->place != up; p = p->next) {}
  if (!p)
    return;
  if (p->prev)
    p->prev->next = p->next;
  else
    up->obj->parents = p->next;
  if (p->next)
    p->next->prev = p->prev;
  free(p);
  up->obj = 0;
}

static void
push_place(struct uiplace *up)
{
  int n, esize = sizeof(struct uiplace *);
  struct uiplace **buf;
  if (0 && stack) {
    buf = (struct uiplace **)stack->b;
    n = stack->used / esize;
    for (;n; --n, ++buf)
      if (*buf == up)
        return;
  }
  if (arr_add(&stack, 16 * esize, esize, &up) < 0)
    die("Cannot allocate memory");
}

static struct uiplace *
pop_place()
{
  if (!stack || stack->used < sizeof(struct uiplace *))
    return 0;
  stack->used -= sizeof(struct uiplace *);
  return *((struct uiplace **)(stack->b + stack->used));
}

static void
show_stack()
{
  struct uiplace **ppu;
  int i, n;
  char *sep = "";

  log_printf(LOG_UI, "stack: [");
  if (stack && stack->used >= sizeof(struct uiplace *)) {
    ppu = (struct uiplace **)stack->b;
    n = stack->used / sizeof(struct uiplace *);
    for (i = 0; i < n; ++i, sep = " ", ++ppu)
      log_printf(LOG_UI, "%s%p", sep, *ppu);
  }
  log_printf(LOG_UI, "]\n");
}

void
ui_propagate_dirty(struct uiplace *up)
{
  struct view *v;
  struct uiobj *u;
  struct uiobj_parent *par;

  log_printf(LOG_UI, ">> ui_propagate_dirty %p\n", up);
  if (stack)
    stack->used = 0;
  push_place(up);
  while (stack && stack->used) {
    up = pop_place();
    if (!up)
      break;
    u = uiplace_container(up);
    if (!u) {
      v = uiplace_container_view(up);
      log_printf(LOG_UI, "  mark view %p as dirty\n", v);
      if (v && (v->flags & VIEW_IS_VISIBLE))
        v->flags |= VIEW_IS_DIRTY;
    } else if (u->frame != framecnt) {
      u->frame = framecnt;
      par = u->parents;
      for (; par; par = par->next)
        push_place(par->place);
    }
  }
}

static void
upd_placement(struct prop *p)
{
  struct uiplace *up = (struct uiplace *)p->aux;
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
    upd_placement(p);
}

int
init_place(struct uiplace *up)
{
  int r;

  r = init_prop_buf(&up->fs, &up->path, "path", 0, "", 0, up)
      | init_prop_buf(&up->fs, &up->sticky, "sticky", 8, 0, 1, up)
      | init_prop_rect(&up->fs, &up->padding, "padding", up)
      | init_prop_rect(&up->fs, &up->place, "place", up);
  up->place.r[0] = -1;
  up->place.r[1] = -1;
  up->place.r[2] = 1;
  up->place.r[3] = 1;

  if (r) {
    free(up->fs.name);
    free(up);
    return -1;
  }
  up->path.p.update = upd_path;
  up->sticky.p.update = upd_placement;
  up->padding.p.update = upd_placement;

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
  if (!up->fs.name || init_place(up)) {
    P9_SET_STR(c->r.ename, "Cannot allocate memory");
    free(up);
    return;
  }
  add_file(&((struct uiobj_container *)u->data)->fs_items, &up->fs);

  log_printf(LOG_UI, ";; done creating place '%s'\n", up->fs.name);
}

void
ui_init_container_items(struct uiobj_container *c, char *name, int single)
{
  memset(&c->fs_items, 0, sizeof(c->fs_items));
  c->fs_items.name = name;
  c->fs_items.owns_name = 0;
  c->fs_items.mode = 0700 | P9_DMDIR;
  c->fs_items.qpath = new_qid(FS_UIPLACE_DIR);
  c->fs_items.fs = &container_fs;
  c->single_item = single;
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

  u = containerof(fid->file, struct uiobj, fs_places);
  cl = (struct client *)u->client;
  par = u->parents;
  for (; par; par = par->next) {
    if (file_path(&buf, &par->place->fs, cl->ui))
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
    read_data_fn(c, buf->used, buf->b);
}

struct p9_fs places_fs = {
  .open = places_open,
  .read = places_read
};

static void
refresh_frames(struct file *root)
{
  struct file *x;
  struct uiobj *u;

  x = root;
  do {
    if (x && FSTYPE(*x) == FS_UIOBJ) {
      u = (struct uiobj *)x;
      u->frame = u->frame1 = framecnt - 1;
    }
    if (FSTYPE(*x) == FS_UIDIR && x->child)
      x = x->child;
    else if (x->next && x != root)
      x = x->next;
    else {
      while (x != root && x->parent && x->parent != root && !x->parent->next)
        x = x->parent;
      if (x != root && x->parent != root && x->parent->next)
        x = x->parent->next;
      else
        break;
    }
  } while (x && x != root);
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
  int a, b;

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
  if (!up || init_place(up))
    return -1;
  v->uiplace = &up->fs;
  return 0;
}

void
ui_free()
{
  if (stack) {
    free(stack);
    stack = 0;
  }
}

void
ui_keyboard(struct view *v, int type, int keysym, int mod,
            unsigned int unicode)
{
}

void
ui_pointer_move(struct view *v, int x, int y, int state)
{
}

void
ui_pointer_click(struct view *v, int x, int y, int btn)
{
}

void
ui_update_view(struct view *v)
{
  struct uiplace *up = (struct uiplace *)v->uiplace;

  log_printf(LOG_UI, ">> ui_update_view '%s'\n", v->fs.name);

  if (!(up && up->obj))
    return;

  walk_view_tree((struct uiplace *)v->uiplace, v, 0, 0, update_place_size);
  wm_view_size_request(v);
  log_printf(LOG_UI, "  view->rect: [%d %d %d %d]\n", v->g.r[0],
             v->g.r[1], v->g.r[2], v->g.r[3]);
  ui_place_with_padding(up, v->g.r);
  ++framecnt;
  walk_view_tree(up, v, 0, resize_place, 0);
  v->flags &= ~VIEW_IS_DIRTY;
}

void
ui_redraw_view(struct view *v)
{
  struct uiplace *up = (struct uiplace *)v->uiplace;

  if (0) log_printf(LOG_UI, ">> ui_redraw_view '%s'\n", v->fs.name);

  if (!(up && up->obj))
    return;

  walk_view_tree((struct uiplace *)v->uiplace, v, 0, 0, draw_obj);
}

void
ui_update()
{
  static const int highestbit = (1 << 31);
  struct client *c;

  if ((framecnt ^ prevframecnt) & highestbit)
    for (c = clients; c; c = c->next)
      refresh_frames(c->ui);
  prevframecnt = framecnt;
}
