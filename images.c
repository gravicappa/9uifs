#include <stdlib.h>
#include <string.h>
#include "9p.h"
#include "fs.h"
#include "fsutil.h"
#include "fstypes.h"
#include "ctl.h"
#include "draw.h"
#include "surface.h"

#define IMG_NAME_PREFIX '_'

struct image_dir {
  struct file f;
};

static void image_create(struct p9_connection *con);

static struct p9_fs imagedir_fs = {
  .create = image_create
};

static void
rm_dir(struct file *dir)
{
  if (dir)
    free(dir);
}

static struct file *
image_mkdir(char *name)
{
  struct image_dir *dir;

  dir = calloc(1, sizeof(struct image_dir));
  if (dir) {
    dir->f.name = name;
    dir->f.mode = 0700 | P9_DMDIR;
    dir->f.qpath = new_qid(FS_IMGDIR);
    dir->f.rm = rm_dir;
    dir->f.fs = &imagedir_fs;
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
  if (name[0] == IMG_NAME_PREFIX) {
    f = (struct file *)mk_surface(0, 0);
  } else
    f = image_mkdir(name);
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
init_image_dir(char *name)
{
  struct file *f;
  f = image_mkdir(name);
  return f;
}
