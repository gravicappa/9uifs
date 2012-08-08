#include <stdlib.h>
#include <string.h>

#include "util.h"
#include "9p.h"
#include "fs.h"
#include "prop.h"
#include "uiobj.h"

struct uiobj_scroll {
  struct uiobj_container c;
  int scroll_pos[2];
};

static void
rm_uiscroll(struct file *f)
{
  ui_rm_uiobj(f);
}

int
init_uiscroll(struct uiobj *u)
{
  struct uiobj_scroll *us;
  u->data = malloc(sizeof(struct uiobj_scroll));
  if (!u->data)
    return -1;
  memset(u->data, 0, sizeof(struct uiobj_scroll));

  u->flags |= UI_IS_CONTAINER;
  u->fs.rm = rm_uiscroll;

  return 0;
}
