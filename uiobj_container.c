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
#include "prop.h"
#include "ui.h"

static void
rm_place(struct file *f)
{
  struct uiobj_place *up = (struct uiobj_place *)f;
  if (!up)
    return;
  if (up->path.buf)
    free(up->path.buf);
  if (up->sticky.buf)
    free(up->sticky.buf);
}

static void
create_place(struct p9_connection *c)
{
  struct uiobj_place *up;
  int r;

  up = (struct uiobj_place *)malloc(sizeof(struct uiobj_place));
  if (!up) {
    P9_SET_STR(c->r.ename, "Cannot allocate memory");
    return;
  }
  memset(up, 0, sizeof(*up));

  up->fs.name = strndup(c->t.name, c->t.name_len);
  if (!up->fs.name) {
    P9_SET_STR(c->r.ename, "Cannot allocate memory");
    free(up);
    return;
  }
  r = init_prop_buf(&up->fs, &up->path, "path", 0, "", 0, up)
      | init_prop_buf(&up->fs, &up->sticky, "sticky", 4, 0, 1, up)
      | init_prop_int(&up->fs, &up->padx, "padx", 0, up)
      | init_prop_int(&up->fs, &up->pady, "pady", 0, up);
  if (r) {
    P9_SET_STR(c->r.ename, "Cannot allocate memory");
    free(up->fs.name);
    free(up);
    return;
  }
  up->fs.owns_name = 1;
  up->fs.mode = 0500 | P9_DMDIR;
  up->fs.qpath = ++qid_cnt;
  up->fs.rm = rm_place;
}

struct p9_fs container_fs = {
  .create = create_place
};
