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
#include "draw.h"
#include "view.h"
#include "prop.h"
#include "uiobj.h"
#include "ui.h"
#include "client.h"

#define UI_NAME_PREFIX '_'

extern int init_uigrid(struct uiobj *);

struct uiobj_maker {
  char *type;
  int (*init)(struct uiobj *);
} uitypes[] = {
  {"grid", init_uigrid},
  /*
  {"button", mk_uibutton},
  {"label", mk_uilabel},
  {"entry", mk_uientry},
  {"blit", mk_uientry},
  */
  {0, 0}
};

extern struct p9_fs places_fs;

static void items_create(struct p9_connection *c);
static void create_place(struct p9_connection *c);

static struct arr *stack = 0;

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
items_mkdir(char *name)
{
  struct file *f;
  f = (struct file *)malloc(sizeof(struct file));
  if (!f)
    return 0;
  memset(f, 0, sizeof(*f));
  f->name = name;
  f->mode = 0700 | P9_DMDIR;
  f->qpath = new_qid(FS_UIDIR);
  f->fs = &items_fs;
  f->rm = rm_dir;
  return f;
}

static void
items_create(struct p9_connection *c)
{
  struct uiobj *u;
  struct file *f, *d;
  char *name;

  if (!(c->t.perm & P9_DMDIR)) {
    P9_SET_STR(c->r.ename, "wrong uiobj create perm");
    return;
  }
  f = (struct file *)c->t.pfid->file;
  name = strndup(c->t.name, c->t.name_len);
  if (!name) {
    P9_SET_STR(c->r.ename, "Cannot allocate memory");
    return;
  }
  if (name[0] == UI_NAME_PREFIX) {
    u = mk_uiobj();
    u->fs.name = name;
    u->fs.owns_name = 1;
    resp_file_create(c, &u->fs);
    u->fs.aux.p = f->aux.p;
    add_file(f, &u->fs);
  } else {
    d = items_mkdir(name);
    d->owns_name = 1;
    d->aux.p = f->aux.p;
    add_file(f, d);
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
    fill_rect(v->blit.img, u->g.x, u->g.y, u->g.w, u->g.h, c);
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
mk_uiobj()
{
  int r;
  struct uiobj *u;

  u = (struct uiobj *)malloc(sizeof(struct uiobj));
  if (!u)
    return 0;
  memset(u, 0, sizeof(*u));
  u->draw = draw_uiobj;
  u->fs.mode = 0500 | P9_DMDIR;
  u->fs.qpath = new_qid(FS_UIOBJ);
  u->fs.rm = ui_rm_uiobj;

  /* TODO: use bg from some kind of style db */
  r = init_prop_buf(&u->fs, &u->type, "type", 0, "", 0, u)
      | init_prop_colour(&u->fs, &u->bg, "background", 0, u)
      | init_prop_int(&u->fs, &u->visible, "visible", 0, u)
      | init_prop_rect(&u->fs, &u->restraint, "restraint", u);

  if (r) {
    rm_file(&u->fs);
    free(u);
    u = 0;
  }

  for (r = 0; r < 4; ++r)
    u->restraint.r[r] = 0;

  u->type.p.fs.fs = &prop_type_fs;

  u->fs_evfilter.name = "evfilter";
  u->fs_evfilter.mode = 0600;
  u->fs_evfilter.qpath = new_qid(0);
  u->fs_evfilter.aux.p = u;
  /*u->fs_evfilter.fs = &evfilter_fs;*/
  add_file(&u->fs, &u->fs_evfilter);

  u->fs_g.name = "g";
  u->fs_g.mode = 0400;
  u->fs_g.qpath = new_qid(0);
  u->fs_g.aux.p = u;
  /*u->fs_g.fs = &geom_fs;*/
  add_file(&u->fs, &u->fs_g);

  u->fs_places.name = "places";
  u->fs_places.mode = 0400;
  u->fs_places.qpath = new_qid(0);
  u->fs_places.aux.p = 0;
  u->fs_places.fs = &places_fs;
  add_file(&u->fs, &u->fs_places);

  return u;
}

static void
update_obj_size(struct uiobj *u)
{
  if (u->update_size)
    u->update_size(u);
  u->req_w = u->restraint.r[0];
  u->req_h = u->restraint.r[1];
}

static struct file *
up_children(struct uiplace *up)
{
  if (!(up->obj && (up->obj->flags & UI_IS_CONTAINER) && up->obj->data))
    return 0;
  return ((struct uiobj_container *)up->obj->data)->fs_items.child;
}

void
ui_update_size(struct view *v, struct uiplace *up)
{
  struct file *f;
  struct uiplace *x, *t, *y;
  int frame;

  log_printf(LOG_UI, ">> ui_update_size\n");

  if (!(up->obj && (up->obj->flags & UI_IS_CONTAINER)))
    return;
  frame = UIOBJ_CLIENT(up->obj)->frame;
  up->parent = 0;
  x = up;
  do {
    f = up_children(x);
    if (f) {
      y = (struct uiplace *)f;
      if (y->obj && y->obj->frame == frame)
        f = 0;
    }
    t = x;
    if (f) {
      x = (struct uiplace *)f;
      x->parent = t;
    } else if (x->fs.next) {
      update_obj_size(x->obj);
      x = (struct uiplace *)x->fs.next;
      x->parent = t->parent;
    } else {
      while (x && x != up && x->parent && !x->parent->fs.next) {
        update_obj_size(x->obj);
        x = x->parent;
      }
      update_obj_size(x->obj);
      if (x->parent && x->parent != up)
        update_obj_size(x->parent->obj);
      if (x != up && x->parent && x->parent->fs.next
          && x->parent->fs.next != &up->obj->fs)
        x = (struct uiplace *)x->parent->fs.next;
    }
  } while (x && x != up);
}

/* update items sizes */
void
update_uiobj(struct uiobj *u)
{
  /*ui_update_size();*/
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
    return UIOBJ_CLIENT(up->obj);
  u = uiplace_container(up);
  if (u)
    return UIOBJ_CLIENT(u);
  v = uiplace_container_view(up);
  if (v)
    return v->c;
  return 0;
}

static void
place_uiobj(struct uiplace *up, struct uiobj *u)
{
  struct uiobj_parent *p, *p1;

  log_printf(LOG_UI, ">> place_uiobj u: %p\n", u);
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
unplace_uiobj(struct uiplace *up)
{
  struct uiobj_parent *p;

  if (!up->obj)
    return;
  log_printf(LOG_UI, ">> unplace_uiobj u: %p\n", up->obj);
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
push_place(struct uiplace *up)
{
  int msize = sizeof(struct uiplace *);
  if (arr_add(&stack, 16 * msize, msize, up) < 0)
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
    log_printf(LOG_UI, "  stack %d\n", stack->used / sizeof(struct uiplace *));
    up = pop_place();
    if (!up)
      break;
    u = uiplace_container(up);
    if (u) {
      par = (struct uiobj_parent *)u->fs_places.aux.p;
      for (; par; par = par->next)
        push_place(par->place);
    } else {
      v = uiplace_container_view(up);
      log_printf(LOG_UI, "  mark view %p as dirty\n", v);
      if (v)
        v->flags |= VIEW_IS_DIRTY;
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
      | init_prop_buf(&up->fs, &up->sticky, "sticky", 4, 0, 1, up)
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
  up->fs.aux.p = up;
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

  u = (struct uiobj *)((struct file *)c->t.pfid->file)->aux.p;
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

  update_uiobj(u);
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
    read_buf_fn(c, buf->used, buf->b);
}

struct p9_fs places_fs = {
  .open = places_open,
  .read = places_read
};

int
ui_init_ui(struct client *c)
{
  c->ui = items_mkdir("ui");
  if (!c->ui)
    return -1;
  c->ui->fs = &root_items_fs;
  c->ui->aux.p = c;
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
ui_update(struct view *v)
{
  ui_update_size(v, (struct uiplace *)v->uiplace);
}
