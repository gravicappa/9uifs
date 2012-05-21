#include <stdlib.h>
#include <string.h>

#include "util.h"
#include "geom.h"
#include "9p.h"
#include "fs.h"
#include "prop.h"
#include "uiobj.h"

struct uiobj_grid {
  struct uiobj_container c;
  int nrows;
  int ncols;
  struct uiplace **grid;
  int *rows_sizes;
  int *cols_sizes;
};

static void
update_grid_grid(struct uiobj_grid *g)
{
  int x, nr, nc;
  struct file *f;
  struct uiplace *up;

  for (nr = nc = 0, f = g->c.fs_items.child; f; f = f->next) {
    up = (struct uiplace *)f;
    if (up->obj) {
      if (up->place.r[0] < 0)
        up->place.r[0] = nc;
      if (up->place.r[2] < 1)
        up->place.r[2] = 1;
      x = up->place.r[0] + up->place.r[2] - 1;
      nc = (nc > x) ? nc : x;

      if (up->place.r[1] < 0)
        up->place.r[1] = nr + 1;
      if (up->place.r[3] < 1)
        up->place.r[3] = 1;
      x = up->place.r[1] + up->place.r[3] - 1;
      nr = (nr > x) ? nr : x;
    }
  }
  if (nc != g->ncols || !g->cols_sizes) {
    if (g->cols_sizes)
      free(g->cols_sizes);
    g->cols_sizes = (int *)malloc(nc * sizeof(int));
  }
  if (nr != g->nrows || !g->rows_sizes) {
    if (g->rows_sizes)
      free(g->rows_sizes);
    g->rows_sizes = (int *)malloc(nr * sizeof(int));
  }
  if (!(g->cols_sizes && g->rows_sizes))
    die("Cannot allocate memory");
  x = nc * nr * sizeof(struct uiplace *);
  if (nc != g->ncols || nr != g->nrows || !g->grid) {
    if (g->grid)
      free(g->grid);
    g->grid = (struct uiplace **)malloc(x);
    if (!g->grid)
      die("Cannot allocate memory");
    g->ncols = nc;
    g->nrows = nr;
  }
  memset(g->grid, 0, x);
  for (f = g->c.fs_items.child; f; f = f->next) {
    up = (struct uiplace *)f;
    if (up->obj)
      g->grid[up->place.r[0] + up->place.r[2] * nc] = up;
  }
}

#define ITER_CELLS(j, nj, i, ni, ref, req, ispan) \
  for (j = 0; j < nj; ++j, ++sizes) { \
    s = 0; \
    for (i = 0; i < ni; ++i) { \
      up = g->grid[ref]; \
      if (up && up->obj && up->place.r[ispan] == 1 && up->obj->req > s) \
        s = up->obj->req; \
    } \
    *sizes = s; \
    pos += s; \
  }

#define FIX_SPANNED(coord, req) \
  for (f = g->c.fs_items.child; f; f = f->next) { \
    up = (struct uiplace *)f; \
    ni = up->place.r[coord + 2]; \
    if (up->obj && ni > 1) { \
      s = 0; \
      j = up->place.r[coord]; \
      for (i = 0; i < ni - 1; ++i) \
        s += sizes[j + i]; \
      if (up->obj->req > s) { \
        ds = up->obj->req / ni; \
        mds = up->obj->req % ni; \
        for (i = 0; i < ni - 1; ++i, --mds) \
          sizes[j + i] = ds + (mds > 0) ? 1 : 0; \
      } \
    } \
  }

static void
update_grid_size(struct uiobj *u)
{
  int i, j, pos, s, ds, mds, *sizes, ni, nj;
  struct uiobj_grid *g = (struct uiobj_grid *)u->data;
  struct uiplace *up;
  struct file *f;

  update_grid_grid(g);
  ni = g->ncols;
  nj = g->nrows;
  pos = 0;
  sizes = g->rows_sizes;
  ITER_CELLS(j, nj, i, ni, i + j * ni, req_w, 2);
  FIX_SPANNED(0, req_w);
  for (s = 0, i = 0; i < ni; ++i)
    s += sizes[i];
  u->req_w = s;
  pos = 0;
  sizes = g->cols_sizes;
  ITER_CELLS(i, ni, j, nj, j + i * ni, req_h, 3);
  FIX_SPANNED(1, req_h);
  for (s = 0, j = 0; j < nj; ++j)
    s += sizes[j];
  u->req_h = s;
}

static void
resize_grid(struct uiobj *u)
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
  ui_rm_uiobj(f);
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
  ui_init_container_items(&g->c, "items");
  g->c.fs_items.aux.p = u;
  add_file(&u->fs, &g->c.fs_items);
  return 0;
}
