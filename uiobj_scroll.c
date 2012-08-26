#include <stdlib.h>
#include <string.h>

#include "util.h"
#include "9p.h"
#include "fs.h"
#include "prop.h"
#include "uiobj.h"

struct uiobj_scroll {
  struct uiplace place;
  struct prop_intarr pos_fs;
  struct prop_intarr expand_fs;
  int pos[2];
  int expand[2];
};

static void
rm_uiscroll(struct file *f)
{
  ui_rm_uiobj(f);
}

int
init_uiscroll(struct uiobj *u)
{
  struct uiobj_scroll *x;

  u->data = 0;
  x = (struct uiobj_scroll *)malloc(sizeof(struct uiobj_scroll));
  if (!x)
    return -1;
  memset(x, 0, sizeof(struct uiobj_scroll));
  x->place.fs.name = "obj";

  if (init_prop_intarr(&u->fs, &x->pos_fs, "scrollpos", 2, x->pos, u)
      || init_prop_intarr(&u->fs, &x->expand_fs, "expand", 2, x->expand, u)
      || ui_init_place(&x->place)) {
    free(x);
    return -1;
  }
  u->data = x;
  u->fs.rm = rm_uiscroll;
  add_file(&u->fs, &x->place.fs);
  return 0;
}
