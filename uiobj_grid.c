#include <stdlib.h>
#include <string.h>

#include "util.h"
#include "geom.h"
#include "9p.h"
#include "fs.h"
#include "prop.h"
#include "uiobj.h"
#include "client.h"

struct uiobj_grid {
  struct uiobj_container c;
  struct file fs_col_conf;
  struct file fs_row_conf;
  int nrows;
  int ncols;
  struct uiplace **grid;
  int *rows_opts;
  int *cols_opts;
};

enum grid_flags {
  UIGRID_DEFAULT_FLAGS = 0,
  UIGRID_IS_EXPAND = 1 << 24,
};

#define UIGRID_CELL_SIZE(x) ((x) & 0x00ffffff)
#define UIGRID_SET_CELL_SIZE(x, s) (((x) & 0xff000000) | ((s) & 0x00ffffff))

static void
update_grid_cellcount(struct uiobj_grid *g, int *pnc, int *pnr)
{
  int x, nr, nc;
  struct file *f;
  struct uiplace *up;

  for (nr = nc = -1, f = g->c.fs_items.child; f; f = f->next) {
    up = (struct uiplace *)f;
    if (up->obj && up->obj->frame != framecnt) {
      if (up->place.r[0] < 0)
        up->place.r[0] = (nc < 0) ? ++nc : nc;
      if (up->place.r[2] < 1)
        up->place.r[2] = 1;
      x = up->place.r[0] + up->place.r[2] - 1;
      nc = (nc > x) ? nc : x;

      if (up->place.r[1] < 0)
        up->place.r[1] = (nr < 0) ? ++nr : nr + 1;
      if (up->place.r[3] < 1)
        up->place.r[3] = 1;
      x = up->place.r[1] + up->place.r[3] - 1;
      nr = (nr > x) ? nr : x;
    }
  }
  ++nc;
  ++nr;
  *pnc = nc;
  *pnr = nr;
}

static void
update_grid_grid(struct uiobj_grid *g)
{
  int x, i, nr, nc;
  struct file *f;
  struct uiplace *up;

  update_grid_cellcount(g, &nc, &nr);

  if (nc != g->ncols || !g->cols_opts) {
    g->cols_opts = (int *)realloc(g->cols_opts, nc * sizeof(int));
    for (i = g->ncols; i < nc; ++i)
      g->cols_opts[i] = UIGRID_DEFAULT_FLAGS;
  }
  if (nr != g->nrows || !g->rows_opts) {
    g->rows_opts = (int *)realloc(g->rows_opts, nr * sizeof(int));
    for (i = g->nrows; i < nr; ++i)
      g->rows_opts[i] = UIGRID_DEFAULT_FLAGS;
  }
  if (!(g->cols_opts && g->rows_opts))
    die("Cannot allocate memory");
  x = nc * nr * sizeof(struct uiplace *);
  if (nc != g->ncols || nr != g->nrows || !g->grid) {
    g->grid = (struct uiplace **)realloc(g->grid, x);
    if (!g->grid)
      die("Cannot allocate memory");
    g->ncols = nc;
    g->nrows = nr;
  }
  memset(g->grid, 0, x);
  for (f = g->c.fs_items.child; f; f = f->next) {
    up = (struct uiplace *)f;
    if (up->obj && up->obj->frame != framecnt) {
      log_printf(LOG_UI, "grid[%d %d] <- %p\n", up->place.r[0],
                 up->place.r[1], up);
      g->grid[up->place.r[0] + up->place.r[1] * nc] = up;
    }
  }
}

static void
iter_cells(struct uiobj_grid *g, int coord)
{
  int i, j, ni, nj, t, s, *opts, mi, mj;
  struct uiplace *up;

  if (coord == 0) {
    opts = g->cols_opts;
    ni = g->nrows;
    nj = g->ncols;
    mi = nj;
    mj = 1;
  } else {
    opts = g->rows_opts;
    ni = g->ncols;
    nj = g->nrows;
    mi = 1;
    mj = ni;
  }
  for (j = 0; j < nj; ++j) {
    s = 0;
    for (i = 0; i < ni; ++i) {
      up = g->grid[i * mi + j * mj];
      log_printf(LOG_UI, "grid %p [%d/%d %d/%d]: %p\n", g, i, ni, j, nj, up);
      if (up && up->obj) {
        t = (up->obj->reqsize[coord] + up->padding.r[coord]
             + up->padding.r[coord + 2]);
        if (up->place.r[coord + 2] == 1 && t > s)
          s = t;
      }
    }
    opts[j] = UIGRID_SET_CELL_SIZE(opts[j], s);
  }
}

static void
iter_spanned_cells(struct uiobj_grid *g, int coord)
{
  int i, j, ni, lim, ds, mds, s, t, *opts;
  struct file *f;
  struct uiplace *up;

  if (coord == 0) {
    opts = g->rows_opts;
    lim = g->nrows;
  } else {
    opts = g->cols_opts;
    lim = g->ncols;
  }
  for (f = g->c.fs_items.child; f; f = f->next) {
    up = (struct uiplace *)f;
    ni = up->place.r[coord + 2];
    if (up->obj && up->obj->frame != framecnt && ni > 1) {
      s = 0;
      j = up->place.r[coord];
      t = (up->obj->reqsize[coord] + up->padding.r[coord]
           + up->padding.r[coord + 2]);
      for (i = 0; i < lim - 1; ++i)
        s += opts[j + i];
      if (t > s) {
        ds = t / lim;
        mds = t % lim;
        for (i = 0; i < ni - 1; ++i, --mds) {
          log_printf(LOG_UI, "> ugs fs i: %d/%d idx: %d\n", i, lim, j + i);
          opts[j + i] = ds + (mds > 0) ? 1 : 0;
        }
      }
    }
  }
}

static void
update_grid_size(struct uiobj *u)
{
  int i, ni, s, *opts;
  struct uiobj_grid *g = (struct uiobj_grid *)u->data;

  log_printf(LOG_UI, ">> update_grid_size '%s'\n", u->fs.name);
  if (!g)
    return;
  update_grid_grid(g);
  if (!(g->ncols && g->nrows))
    return;
  log_printf(LOG_UI, "> ugs grid size: [%d %d]\n", g->ncols, g->nrows);

  iter_cells(g, 0);
  iter_spanned_cells(g, 0);
  iter_cells(g, 1);
  iter_spanned_cells(g, 1);

  opts = g->cols_opts;
  ni = g->ncols;
  for (s = 0, i = 0; i < ni; ++i)
    s += opts[i];
  u->reqsize[0] = s;
  opts = g->rows_opts;
  ni = g->nrows;
  for (s = 0, i = 0; i < ni; ++i)
    s += opts[i];
  u->reqsize[1] = s;
}

static void
resize_grid_dim(int ndims, int *dims, int req, int dim)
{
  int dx, x, i, n, rem;
  int *pd;

  for (pd = dims, x = 0, n = 0, i = 0; i < ndims; ++i, ++pd)
    if (*pd & UIGRID_IS_EXPAND)
      ++n;
  if (dim >= req && n) {
    pd = dims;
    x = dim - req;
    dx = x / n;
    rem = x % n;
    for (pd = dims, i = 0; i < ndims; ++i, ++pd, --rem)
      if (*pd & UIGRID_IS_EXPAND)
        *pd += dx + ((rem > 0) ? 1 : 0);
  }
}

static void
resize_grid(struct uiobj *u)
{
  int x, y, w, h, i, ni, j, nj, a, na, *pcolw, *prowh, f;
  int r[4];
  struct uiobj_grid *g = (struct uiobj_grid *)u->data;
  struct uiplace *up;

  if (!(g && g->nrows && g->ncols))
    return;
  w = u->g.r[2];
  h = u->g.r[3];
  ni = g->ncols;
  nj = g->nrows;

  log_printf(LOG_UI, ">> resize_grid '%s'\n", u->fs.name);

  resize_grid_dim(ni, g->cols_opts, u->reqsize[0], w);
  resize_grid_dim(nj, g->rows_opts, u->reqsize[1], h);

  prowh = g->rows_opts;
  for (y = u->g.r[1], j = 0; j < nj; ++j, ++prowh) {
    log_printf(LOG_UI, "  - rowh[%d/%d]: %d\n", j, nj, *prowh);
    pcolw = g->cols_opts;
    for (x = u->g.r[0], i = 0; i < ni; ++i, ++pcolw) {
      up = g->grid[i + j * ni];
      if (up && up->obj) {
        r[0] = x;
        r[1] = y;
        w = UIGRID_CELL_SIZE(*pcolw);
        h = UIGRID_CELL_SIZE(*prowh);
        na = up->place.r[2];
        for (a = 1; a < na; ++a)
          w += UIGRID_CELL_SIZE(pcolw[a]);
        na = up->place.r[3];
        for (a = 1; a < na; ++a)
          h += UIGRID_CELL_SIZE(prowh[a]);
        r[2] = w;
        r[3] = h;
        if (up->obj->frame != framecnt)
          ui_place_with_padding(up, r);
      }
      x += UIGRID_CELL_SIZE(*pcolw);
    }
    y += UIGRID_CELL_SIZE(*prowh);
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
