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
#include "ui.h"

#define UI_NAME_PREFIX '_'

static void items_create(struct p9_connection *c);

struct p9_fs items_fs = {
  .create = items_create,
};

struct p9_fs root_items_fs = {
  .create = items_create
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

struct file *
mk_ui(char *name, void *aux)
{
  struct file *f;
  f = items_mkdir(name);
  f->fs = &root_items_fs;
  f->aux.p = aux;
  return f;
}
