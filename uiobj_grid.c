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

  for (nr = nc = -1, f = g->c.fs_items.child; f; f = f->next) {
    up = (struct uiplace *)f;
    if (up->obj) {
      if (up->place.r[0] < 0)
        up->place.r[0] = nc + 1;
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
  ++nc;
  ++nr;
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
      g->grid[up->place.r[0] + up->place.r[1] * nc] = up;
  }
}

#define ITER_CELLS(j, nj, i, ni, ref, req, coord) \
  for (j = 0; j < nj; ++j) { \
    s = 0; \
    for (i = 0; i < ni; ++i) { \
      up = g->grid[ref]; \
      t = up->obj->req + up->padding.r[coord] + up->padding.r[coord + 2]; \
      if (up && up->obj && up->place.r[coord + 2] == 1 && t > s) \
        s = t; \
    } \
    sizes[j] = s; \
    pos += s; \
  }

#define FIX_SPANNED(lim, req, coord) \
  for (f = g->c.fs_items.child; f; f = f->next) { \
    up = (struct uiplace *)f; \
    ni = up->place.r[coord + 2]; \
    if (up->obj && ni > 1) { \
      s = 0; \
      j = up->place.r[coord]; \
      t = up->obj->req + up->padding.r[coord] + up->padding.r[coord + 2]; \
      for (i = 0; i < lim - 1; ++i) \
        s += sizes[j + i]; \
      if (t > s) { \
        ds = t / lim; \
        mds = t % lim; \
        for (i = 0; i < ni - 1; ++i, --mds) { \
          log_printf(LOG_UI, "> ugs fs i: %d/%d idx: %d\n", i, lim, j + i); \
          sizes[j + i] = ds + (mds > 0) ? 1 : 0; \
        } \
      } \
    } \
  }

static void
update_grid_size(struct uiobj *u)
{
  int i, j, pos, s, ds, mds, *sizes, ni, nj, t;
  struct uiobj_grid *g = (struct uiobj_grid *)u->data;
  struct uiplace *up;
  struct file *f;

  log_printf(LOG_UI, ">> update_grid_size '%s'\n", u->fs.name);
  update_grid_grid(g);
  ni = g->ncols;
  nj = g->nrows;
  pos = 0;
  sizes = g->rows_sizes;
  ITER_CELLS(j, nj, i, ni, i + j * ni, req_h, 0);
  FIX_SPANNED(nj, req_h, 0);
  log_printf(LOG_UI, "> ugs grid size: [%d %d]\n", g->ncols, g->nrows);
  log_printf(LOG_UI, "> ugs sizes: %p g->cols_sizes: %p\n", sizes,
             g->cols_sizes);
  for (s = 0, i = 0; i < ni; ++i) {
    log_printf(LOG_UI, "> ugs i: %d/%d\n", i, ni);
    s += sizes[i];
  }
  log_printf(LOG_UI, "> ugs s: %d\n", s);
  u->req_w = s;
  pos = 0;
  sizes = g->rows_sizes;
  ITER_CELLS(i, ni, j, nj, j + i * ni, req_w, 1);
  FIX_SPANNED(ni, req_w, 0);
  for (s = 0, j = 0; j < nj; ++j)
    s += sizes[j];
  u->req_h = s;
}

static void
resize_grid(struct uiobj *u)
{
  int x, y, w, h, i, ni, j, nj, a, na, *pcolw, *prowh, dw, rem, dh;
  int r[4];
  struct uiobj_grid *g = (struct uiobj_grid *)u->data;
  struct uiplace *up;

  if (!g)
    return;
  x = u->g.r[0];
  w = u->g.r[2];
  h = u->g.r[3];
  nj = g->nrows;
  ni = g->ncols;
  dh = u->req_h - h;

  pcolw = g->cols_sizes;
  dw = w / g->ncols;
  rem = w % g->ncols;
  for (i = 0; i < ni; ++i, ++pcolw, --rem)
    *pcolw = dw + (rem > 0) ? 1 : 0;

  dh = h / g->nrows;
  rem = h % g->nrows;
  prowh = g->rows_sizes;
  for (y = u->g.r[1], j = 0; j < nj; ++j, ++prowh, --rem) {
    *prowh = dh + (rem > 0) ? 1 : 0;
    pcolw = g->cols_sizes;
    for (x = u->g.r[0], i = 0; i < ni; ++i, ++pcolw) {
      up = g->grid[i + j * ni];
      if (up->obj) {
        r[0] = x;
        r[1] = y;
        w = *pcolw;
        h = *prowh;
        na = up->place.r[2];
        for (a = 1; a < na; ++a) 
          w += pcolw[a];
        na = up->place.r[3];
        for (a = 1; a < na; ++a) 
          h += prowh[a];
        r[2] = w;
        r[3] = h;
        ui_place_with_padding(up, r);
      }
      x += *pcolw;
    }
    y += *prowh;
  }
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
  memset(u->data, 0, sizeof(struct uiobj_grid));
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
