#include <stdlib.h>
#include <string.h>
#include "9p.h"
#include "fs.h"
#include "fsutil.h"
#include "fstypes.h"
#include "ctl.h"
#include "frontend.h"
#include "image.h"

#define IMG_NAME_PREFIX '_'

struct image_dir {
  struct file f;
  struct file *libroot;
};

static void image_create(struct p9_connection *con);

static struct p9_fs imagedir_fs = {
  .create = image_create
};

static struct file *
image_mkdir(char *name, struct file *libroot)
{
  struct image_dir *dir;

  dir = calloc(1, sizeof(struct image_dir));
  if (dir) {
    dir->f.name = name;
    dir->f.mode = 0700 | P9_DMDIR;
    dir->f.qpath = new_qid(FS_IMGDIR);
    dir->f.rm = free_file;
    dir->f.fs = &imagedir_fs;
    dir->libroot = libroot;
  }
  return (struct file *)dir;
}

static void
image_create(struct p9_connection *con)
{
  struct image_dir *dir;
  char *name;
  struct file *f;

  if (!(con->t.perm & P9_DMDIR)) {
    P9_SET_STR(con->r.ename, "wrong image create permissions");
    return;
  }
  dir = con->t.pfid->file;
  name = strndup(con->t.name, con->t.name_len);
  if (!name) {
    P9_SET_STR(con->r.ename, "Cannot allocate memory");
    return;
  }
  if (name[0] == IMG_NAME_PREFIX)
    f = (struct file *)mk_image(0, 0, dir->libroot, con);
  else
    f = image_mkdir(name, dir->libroot);
  if (!f) {
    P9_SET_STR(con->r.ename, "Cannot allocate memory");
    return;
  }
  f->name = name;
  f->owns_name = 1;
  resp_file_create(con, f);
  add_file(&dir->f, f);
}

struct file *
mk_image_dir(char *name)
{
  struct image_dir *dir;
  dir = (struct image_dir *)image_mkdir(name, 0);
  dir->libroot = (struct file *)dir;
  return (struct file *)dir;
}
