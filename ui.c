#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <Imlib2.h>

#include "util.h"
#include "9p.h"
#include "fs.h"
#include "fsutil.h"
#include "geom.h"
#include "event.h"
#include "ctl.h"
#include "surface.h"
#include "view.h"
#include "ui.h"

#define INT_BUF_SIZE 16

static void
int_free(struct p9_fid *fid)
{
  log_printf(4, ";; int-free fid->aux: %p\n", fid->aux);
  if (fid->aux)
    free(fid->aux);
}

static void
int_open(struct p9_connection *c)
{
  struct uiprop *p;
  struct p9_fid *fid = c->t.pfid;

  p = (struct uiprop *)fid->file;
  fid->aux = malloc(INT_BUF_SIZE);
  log_printf(4, ";; int-open fid->aux: %p\n", fid->aux);
  memset(fid->aux, 0, INT_BUF_SIZE);
  fid->rm = int_free;
  if (!(c->t.mode & P9_OTRUNC))
    snprintf((char *)fid->aux, INT_BUF_SIZE, "%d", p->d.i);
}

static void
int_read(struct p9_connection *c)
{
  read_buf_fn(c, strlen((char *)c->t.pfid->aux), (char *)c->t.pfid->aux);
}

static void
int_write(struct p9_connection *c)
{
  write_buf_fn(c, INT_BUF_SIZE - 1, (char *)c->t.pfid->aux);
}

static void
int_clunk(struct p9_connection *c)
{
  int x;
  struct uiprop *p;

  if (!c->t.pfid->aux)
    return;

  p = (struct uiprop *)c->t.pfid->file;
  if (sscanf((char *)c->t.pfid->aux, "%d", &x) != 1) {
    P9_SET_STR(c->r.ename, "Wrong number format");
    return;
  }
  if (p)
    p->d.i = x;
}

struct p9_fs int_fs = {
  .open = int_open,
  .read = int_read,
  .write = int_write,
  .clunk = int_clunk
};

struct p9_fs buf_fs = {
  /*
  .open = int_open,
  .read = int_read,
  .write = int_write,
  .clunk = int_clunk
  */
};

struct p9_fs str_fs = {
  /*
  .open = int_open,
  .read = int_read,
  .write = int_write,
  .clunk = int_clunk
  */
};

static void
init_prop(struct uiobj *u, struct uiprop *p, char *name, int type)
{
  memset(p, 0, sizeof(*p));
  p->type = type;
  p->obj = u;
  p->fs.name = name;
  p->fs.mode = 0600;
  p->fs.qpath = ++qid_cnt;
  p->fs.aux.p = p;
}

int
ui_init_prop_int(struct uiobj *u, struct uiprop *p, char *name, int x)
{
  init_prop(u, p, name, UI_PROP_INT);
  p->d.i = x;
  p->fs.fs = &int_fs;
  add_file(&u->fs, &p->fs);
  return 0;
}

int
ui_init_prop_ptr(struct uiobj *u, struct uiprop *p, char *name, void *ptr)
{
  init_prop(u, p, name, UI_PROP_PTR);
  p->fs.mode = 0400;
  p->d.p = ptr;
  return 0;
}

int
ui_init_prop_buf(struct uiobj *u, struct uiprop *p, char *name, int size,
                 char *x)
{
  int off;

  init_prop(u, p, name, UI_PROP_BUF);
  p->d.buf.delta = size;
  off = add_data(&p->d.buf, size, x);
  if (off < 0)
    return -1;
  if (!x)
    memset(p->d.buf.b + off, 0, size);
  p->d.buf.delta = 0;
  p->fs.fs = &buf_fs;
  add_file(&u->fs, &p->fs);
  return 0;
}

int
ui_init_prop_str(struct uiobj *u, struct uiprop *p, char *name, int size,
                 char *x)
{
  int off;

  init_prop(u, p, name, UI_PROP_STR);
  p->d.buf.delta = 16;
  off = add_data(&p->d.buf, size, x);
  if (off < 0)
    return -1;
  if (!x)
    memset(p->d.buf.b + off, 0, size);
  p->fs.fs = &str_fs;
  add_file(&u->fs, &p->fs);
  return 0;
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
  u->fs.mode = 0500 | P9_DMDIR;
  u->fs.qpath = ++qid_cnt;

  r = ui_init_prop_buf(u, &u->bg, "background", 8, "00000000")
      | ui_init_prop_int(u, &u->visible, "visible", 0)
      | ui_init_prop_int(u, &u->drawable, "drawable", 0)
      | ui_init_prop_int(u, &u->minwidth, "minwidth", 0)
      | ui_init_prop_int(u, &u->maxwidth, "maxwidth", 0)
      | ui_init_prop_int(u, &u->minheight, "minheight", 0)
      | ui_init_prop_int(u, &u->maxheight, "maxheight", 0)
      | ui_init_prop_int(u, &u->padx, "padx", 0)
      | ui_init_prop_int(u, &u->pady, "pady", 0)
      | ui_init_prop_buf(u, &u->sticky, "sticky", 4, "");

  if (r) {
    rm_file(&u->fs);
    free(u);
    u = 0;
  }
  return u;
}

struct uiobj *
mk_ui(struct file *root, char *name, void *aux)
{
  struct uiobj *u = mk_uiobj();

  if (!u)
    return 0;

  u->fs.name = name;
  u->fs.aux.p = aux;
  add_file(root, &u->fs);
  return u;
}
