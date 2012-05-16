#include <stdlib.h>

#include "util.h"
#include "geom.h"
#include "9p.h"
#include "fs.h"
#include "prop.h"
#include "ui.h"

struct uiobj_grid {
  struct uiobj_container c;
};

#if 0
static void
calc_grid_size(struct uiobj *uo, int *rows, int *cols)
{
  int i, j, x;
  struct file *f;
  struct uiobj *o;

  *rows = 0;
  *cols = 0;

  for (f = ui->fs_items.child; f; f = f->next) {
    o = (struct uiobj *)f->aux.p;
    x = o->place.col + o->place.colspan;
    *cols = (*cols > x) ? *cols : x;
    x = o->place.row + o->place.rowspan;
    *rows = (*rows > x) ? *rows : y;
    if (o->update_size)
      o->update_size(o);
  }
}


static void
update_placement_rec(struct uiobj *uo, int x, int y, int w, int h)
{
  int rows, cols;

  calc_grid_size(uo, &rows, &cols);
}
#endif

static void
resize_grid(struct uiobj *u)
{
  ;
}

static void
update_grid_size(struct uiobj *u)
{
  ;
}

void
rm_uigrid(struct file *f)
{
  struct uiobj *u = (struct uiobj *)f;
  log_printf(LOG_DBG, "rm_uigrid %p data: %p\n", u, u->data);
  if (u->data) {
    free(u->data);
    u->data = 0;
  }
  rm_uiobj(f);
}

int
init_uigrid(struct uiobj *u)
{
  struct uiobj_grid *g;
  u->data = malloc(sizeof(struct uiobj_grid));
  if (!u->data)
    return -1;
  u->resize = resize_grid;
  u->update_size = update_grid_size;
  u->flags |= UI_IS_CONTAINER;
  u->fs.rm = rm_uigrid;
  g = (struct uiobj_grid *)u->data;
  init_container_items(&g->c, "items");
  g->c.fs_items.aux.p = u;
  add_file(&u->fs, &g->c.fs_items);
  return 0;
}
