#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "util.h"
#include "geom.h"
#include "9p.h"
#include "fs.h"
#include "fsutil.h"
#include "prop.h"
#include "uiobj.h"
#include "event.h"
#include "client.h"

struct gridopt {
  struct file f;
  struct uiobj_grid *grid;
  int coord;
};

struct uiobj_grid {
  struct uiobj_container c;
  struct gridopt f_cols_opts;
  struct gridopt f_rows_opts;
  int nrows;
  int ncols;
  struct uiplace **grid;
  int *rows_opts;
  int *cols_opts;
};

#define UIGRID_EXPAND (1 << 24)
#define UIGRID_DEFAULT_FLAGS 0

#define UIGRID_CELL_SIZE(x) ((x) & 0x00ffffff)
#define UIGRID_SET_CELL_SIZE(x, s) (((x) & 0xff000000) | ((s) & 0x00ffffff))
#define UIGRID_SET_CELL_FLAGS(x, f) (((x) & 0x00ffffff) | ((f) & 0xff000000))

static void
update_grid_cellcount(struct uiobj_grid *g, int *pnc, int *pnr)
{
  int x, nr, nc;
  struct file *f;
  struct uiplace *up;

  for (nr = nc = 0, f = g->c.f_items.child; f; f = f->next) {
    up = (struct uiplace *)f;
    if (up->obj) {
      if (up->place.r[0] < 0)
        up->place.r[0] = (nc > 0) ? nc - 1: nc;
      if (up->place.r[2] < 1)
        up->place.r[2] = 1;
      x = up->place.r[0] + up->place.r[2];
      nc = (nc > x) ? nc : x;

      if (up->place.r[1] < 0)
        up->place.r[1] = nr;
      if (up->place.r[3] < 1)
        up->place.r[3] = 1;
      x = up->place.r[1] + up->place.r[3];
      nr = (nr > x) ? nr : x;
    }
  }
  *pnc = nc;
  *pnr = nr;
}

static int
update_grid_opts(struct uiobj_grid *g, int ncols, int nrows)
{
  int i;

  if (ncols != g->ncols || !g->cols_opts) {
    g->cols_opts = (int *)realloc(g->cols_opts, ncols * sizeof(int));
    if (ncols > 0 && g->cols_opts == 0)
      return -1;
    for (i = g->ncols; i < ncols; ++i)
      g->cols_opts[i] = UIGRID_DEFAULT_FLAGS;
  }
  if (nrows != g->nrows || !g->rows_opts) {
    g->rows_opts = (int *)realloc(g->rows_opts, nrows * sizeof(int));
    if (nrows > 0 && g->rows_opts == 0)
      return -1;
    for (i = g->nrows; i < nrows; ++i)
      g->rows_opts[i] = UIGRID_DEFAULT_FLAGS;
  }
  return 0;
}

static void
update_grid_grid(struct uiobj_grid *g)
{
  int x, nr, nc;
  struct file *f;
  struct uiplace *up;

  update_grid_cellcount(g, &nc, &nr);
  if (update_grid_opts(g, nc, nr))
    die("Cannot allocate memory [update-grid-grid]");
  x = nc * nr * sizeof(struct uiplace *);
  log_printf(LOG_UI, "update_grid_grid [%d %d]\n", nc, nr);
  if (nc != g->ncols || nr != g->nrows || !g->grid) {
    g->grid = (struct uiplace **)realloc(g->grid, x);
    if (nc > 0 && nr > 0 && !g->grid)
      die("Cannot allocate memory [update-grid-grid realloc grid]");
    g->ncols = nc;
    g->nrows = nr;
  }
  memset(g->grid, 0, x);
  for (f = g->c.f_items.child; f; f = f->next) {
    up = (struct uiplace *)f;
    if (up->obj)
      g->grid[up->place.r[0] + up->place.r[1] * nc] = up;
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
  for (f = g->c.f_items.child; f; f = f->next) {
    up = (struct uiplace *)f;
    ni = up->place.r[coord + 2];
    if (up->obj && ni > 1) {
      s = 0;
      j = up->place.r[coord];
      t = (up->obj->reqsize[coord] + up->padding.r[coord]
           + up->padding.r[coord + 2]);
      for (i = 0; i < lim - 1; ++i)
        s += opts[j + i];
      if (t > s) {
        ds = t / lim;
        mds = t % lim;
        for (i = 0; i < ni - 1; ++i, --mds)
          opts[j + i] = ds + (mds > 0) ? 1 : 0;
      }
    }
  }
}

static void
update_grid_size(struct uiobj *u)
{
  int i, ni, s, *opts;
  struct uiobj_grid *g = (struct uiobj_grid *)u->data;

  log_printf(LOG_UI, "# update_grid_size g: %p\n", g);
  if (!g)
    return;
  update_grid_grid(g);
  if (!(g->ncols && g->nrows))
    return;

  iter_cells(g, 0);
  iter_spanned_cells(g, 0);
  iter_cells(g, 1);
  iter_spanned_cells(g, 1);

  opts = g->cols_opts;
  ni = g->ncols;
  for (s = 0, i = 0; i < ni; ++i)
    s += UIGRID_CELL_SIZE(opts[i]);
  u->reqsize[0] = s;
  opts = g->rows_opts;
  ni = g->nrows;
  for (s = 0, i = 0; i < ni; ++i)
    s += UIGRID_CELL_SIZE(opts[i]);
  u->reqsize[1] = s;
}

static void
resize_grid_dim(int ndims, int *dims, int req, int dim)
{
  int dx, x, i, n, rem;
  int *pd;

  for (pd = dims, x = 0, n = 0, i = 0; i < ndims; ++i, ++pd)
    if (*pd & UIGRID_EXPAND)
      ++n;
  if (dim >= req && n) {
    pd = dims;
    x = dim - req;
    dx = x / n;
    rem = x % n;
    for (pd = dims, i = 0; i < ndims; ++i, ++pd, --rem)
      if (*pd & UIGRID_EXPAND)
        *pd += dx + ((rem > 0) ? 1 : 0);
  }
}

static void
resize_grid(struct uiobj *u)
{
  int x, y, w, h, i, ni, j, nj, a, na, *pcolw, *prowh;
  int r[4];
  struct uiobj_grid *g = (struct uiobj_grid *)u->data;
  struct uiplace *up;

  if (!(g && g->nrows && g->ncols))
    return;

  w = u->g.r[2];
  h = u->g.r[3];
  ni = g->ncols;
  nj = g->nrows;

  resize_grid_dim(ni, g->cols_opts, u->reqsize[0], w);
  resize_grid_dim(nj, g->rows_opts, u->reqsize[1], h);

  prowh = g->rows_opts;
  for (y = u->g.r[1], j = 0; j < nj; ++j, ++prowh) {
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
        log_printf(LOG_UI, "grid %s place %s [%d %d %d %d]\n",
                   u->f.name, up->obj->f.name, r[0], r[1], r[2], r[3]);
        log_printf(LOG_UI, "  reqsize: [%d %d]\n",
                   up->obj->reqsize[0], up->obj->reqsize[1]);
        log_printf(LOG_UI, "  padding: [%d %d %d %d]\n",
                   up->padding.r[0], up->padding.r[1], up->padding.r[2],
                   up->padding.r[3]);
        ui_place_with_padding(up, r);
        log_printf(LOG_UI, "  [%d %d %d %d]\n",
                   up->obj->g.r[0], up->obj->g.r[1], up->obj->g.r[2],
                   up->obj->g.r[3]);
      }
      x += UIGRID_CELL_SIZE(*pcolw);
    }
    y += UIGRID_CELL_SIZE(*prowh);
  }
  log_printf(LOG_UI, "grid resize done\n");
}

static void
rm_uigrid(struct file *f)
{
  struct uiobj *u = (struct uiobj *)f;
  if (u->data) {
    free(u->data);
    u->data = 0;
  }
  ui_rm_uiobj(f);
}

static void
update_grid(struct uiobj_grid *g)
{
  struct file *f;
  for (f = g->c.f_items.child; f; f = f->next)
    ui_propagate_dirty((struct uiplace *)f);
}

static void
opts_rmfid(struct p9_fid *fid)
{
  if (fid->aux)
    free(fid->aux);
  fid->rm = 0;
  fid->aux = 0;
}

static void
opts_open(struct p9_connection *c)
{
  struct p9_fid *fid = c->t.pfid;
  struct gridopt *go = (struct gridopt *)fid->file;
  struct uiobj_grid *g = go->grid;
  int i, n, off, *opts, len, ncols, nrows;
  struct arr *a = 0;
  char buf[16], *sep = "";

  fid->rm = opts_rmfid;
  fid->aux = 0;

  update_grid_cellcount(g, &ncols, &nrows);
  if (update_grid_opts(g, ncols, nrows)) {
    P9_SET_STR(c->r.ename, "Cannot allocate memory");
    return;
  }
  go->grid->ncols = ncols;
  go->grid->nrows = nrows;

  if (go->coord == 0) {
    opts = go->grid->cols_opts;
    n = go->grid->ncols;
  } else {
    opts = go->grid->rows_opts;
    n = go->grid->nrows;
  }
  off = 0;
  for (i = 0; i < n; ++i, ++opts) {
    len = snprintf(buf, sizeof(buf), "%s%d", sep, *opts >> 24);
    if (arr_memcpy(&a, 16, off, len + 1, buf) < 0) {
      P9_SET_STR(c->r.ename, "out of memory");
      return;
    }
    off += len;
    sep = " ";
  }
  fid->aux = a;
}

static void
opts_clunk(struct p9_connection *c)
{
  struct p9_fid *fid = c->t.pfid;
  struct gridopt *go = (struct gridopt *)fid->file;
  int i, n, *opts, x;
  struct arr *buf = (struct arr *)fid->aux;
  char *s, *a;
  int mode = c->t.pfid->open_mode;

  if (!buf || ((mode & 3) != P9_OWRITE && (mode & 3) != P9_ORDWR))
    return;

  if (go->coord == 0) {
    opts = go->grid->cols_opts;
    n = go->grid->ncols;
  } else {
    opts = go->grid->rows_opts;
    n = go->grid->nrows;
  }

  s = buf->b;
  for (i = 0, a = next_arg(&s); i < n && a; ++i, a = next_arg(&s), ++opts)
    if (sscanf(a, "%d", &x) == 1)
      *opts = UIGRID_SET_CELL_FLAGS(*opts, (x << 24));
  free(buf);
  fid->aux = 0;
  update_grid(go->grid);
}

static void
opts_read(struct p9_connection *c)
{
  struct p9_fid *fid = c->t.pfid;
  struct arr *buf = (struct arr *)fid->aux;
  if (buf)
    read_data_fn(c, strlen(buf->b), (char *)buf->b);
}

static void
opts_write(struct p9_connection *c)
{
  struct p9_fid *fid = c->t.pfid;
  struct arr *buf = (struct arr *)fid->aux;
  struct gridopt *go = (struct gridopt *)fid->file;

  if (buf)
    write_buf_fn(c, 16, &buf);
  else if ((go->coord == 0 && go->grid->ncols == 0)
           || (go->coord == 1 && go->grid->nrows == 0))
    c->r.count = c->t.count;
}

struct p9_fs opts_fs = {
  .open = opts_open,
  .read = opts_read,
  .write = opts_write,
  .clunk = opts_clunk
};

static struct file *
get_children(struct uiobj *u)
{
  struct uiobj_container *c = (struct uiobj_container *)u->data;
  return (c) ? c->f_items.child : 0;
}

static struct uiobj_ops grid_ops = {
  .resize = resize_grid,
  .update_size = update_grid_size,
  .draw = default_draw_uiobj,
  .get_children = get_children,
};

int
init_uigrid(struct uiobj *u)
{
  struct uiobj_grid *g;
  u->data = calloc(1, sizeof(struct uiobj_grid));
  if (!u->data)
    return -1;
  g = (struct uiobj_grid *)u->data;

  g->f_cols_opts.coord = 0;
  g->f_cols_opts.grid = g;
  g->f_cols_opts.f.name = "colsopts";
  g->f_cols_opts.f.mode = 0600;
  g->f_cols_opts.f.qpath = new_qid(0);
  g->f_cols_opts.f.fs = &opts_fs;
  add_file(&u->f, &g->f_cols_opts.f);


  g->f_rows_opts.coord = 1;
  g->f_rows_opts.grid = g;
  g->f_rows_opts.f.name = "rowsopts";
  g->f_rows_opts.f.mode = 0600;
  g->f_rows_opts.f.qpath = new_qid(0);
  g->f_rows_opts.f.fs = &opts_fs;
  add_file(&u->f, &g->f_rows_opts.f);

  ui_init_container_items(&g->c, "items");
  g->c.u = u;
  add_file(&u->f, &g->c.f_items);
  u->ops = &grid_ops;
  u->flags |= UI_CONTAINER;
  u->f.rm = rm_uigrid;

  return 0;
}
