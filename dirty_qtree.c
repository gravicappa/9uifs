#include <stdio.h>
#include <string.h>

#include "dirty.h"
#include "util.h"

struct qleaf {
  int mask;
  int r[4];
  struct qleaf *leaves[4];
};

static int qrect[4] = {0}, qused = 0;
static struct qleaf *draw_qtree = 0;
static int base[4];

#define QDEPTH 4
#define NLEAVES(depth) (((1 << (depth << 1)) - 1) / (4 - 1))
#define QSIZE (1 << (QDEPTH - 1))

static struct qleaf leaves[NLEAVES(QDEPTH)] = {{0}};
static int dirty_map[QSIZE * QSIZE];
static int dirty_row[QSIZE];
int dirty_rects[QSIZE * QSIZE * 4];
int ndirty_rects = 0;

static void
print_qtree_leaf(struct qleaf *q, int level)
{
  int i = level;

  while (i--)
    log_printf(LOG_DBG, "  ");
  log_printf(LOG_DBG, "%d%d%d%d [%d %d %d %d]\n",
             !!(q->mask & 8), !!(q->mask & 4), !!(q->mask & 2), q->mask & 1,
             q->r[0], q->r[1], q->r[2], q->r[3]);
  for (i = 0; i < 4; ++i)
    if (q->leaves[i])
      print_qtree_leaf(q->leaves[i], level + 1);
}

static struct qleaf *
ensure_qleaf(struct qleaf *root, int id)
{
  struct qleaf *q = root->leaves[id];
  int i, mx, my, *r = root->r;

  if (q)
    return q;
  if (qused >= NLEAVES(QDEPTH)) {
    log_printf(LOG_DBG, "qtree quota exceeded\n");
    return 0;
  }
  q = root->leaves[id] = &leaves[qused++];
  for (i = 0; i < 4; ++i)
    q->leaves[i] = 0;
  q->mask = 0;
  mx = r[2] >> 1;
  my = r[3] >> 1;
  q->r[2] = mx + (r[2] & 1);
  q->r[3] = my + (r[3] & 1);
  switch (id) {
  case 0:
    q->r[0] = r[0];
    q->r[1] = r[1];
    break;
  case 1:
    q->r[0] = r[0] + mx;
    q->r[1] = r[1];
    break;
  case 2:
    q->r[0] = r[0];
    q->r[1] = r[1] + my;
    break;
  case 3:
    q->r[0] = r[0] + mx;
    q->r[1] = r[1] + my;
    break;
  }
  root->mask |= (1 << id);
  return q;
}

void
init_dirty(int x, int y, int w, int h)
{
  base[0] = qrect[0] = x;
  base[1] = qrect[1] = y;
  base[2] = qrect[2] = w;
  base[3] = qrect[3] = h;
  qused = 0;
  draw_qtree = 0;
}

void
set_dirty_base_rect(int r[4])
{
  int i;
  for (i = 0; i < 4; ++i)
    base[i] = r[i];
}

static void
mark_dirty_aux(int level, struct qleaf *q, int r[4])
{
  int i, m[2], *qr = q->r;

  if (level >= QDEPTH)
    return;
  for (i = 0; i < 2; ++i)
    m[i] = qr[i] + (qr[i + 2] >> 1);
  if (r[0] <= m[0]) {
    if (r[1] <= m[1])
      mark_dirty_aux(level + 1, ensure_qleaf(q, 0), r);
    if (r[1] + r[3] >= m[1])
      mark_dirty_aux(level + 1, ensure_qleaf(q, 2), r);
  }
  if (r[0] + r[2] >= m[0]) {
    if (r[1] <= m[1])
      mark_dirty_aux(level + 1, ensure_qleaf(q, 1), r);
    if (r[1] + r[3] >= m[1])
      mark_dirty_aux(level + 1, ensure_qleaf(q, 3), r);
  }
}

void
add_dirty_rect(int r[4])
{
  int i;
  if (r[0] > base[2] || r[1] > base[3] || r[0] + r[2] < 0 || r[1] + r[3] < 0)
    return;
  r[0] += base[0];
  r[1] += base[1];
  if (!draw_qtree) {
    qused = 0;
    draw_qtree = &leaves[qused++];
    draw_qtree->mask = 0;
    for (i = 0; i < 4; ++i) {
      draw_qtree->r[i] = qrect[i];
      draw_qtree->leaves[i] = 0;
    }
  }
  mark_dirty_aux(1, draw_qtree, r);
}

void
add_dirty_rect2(int x, int y, int w, int h)
{
  int r[4] = {x, y, w, h};
  add_dirty_rect(r);
}

static void
iterate_qtree(struct qleaf *q, void (*fn)(int r[4], void *aux), void *aux)
{
  int i, m;

  if (q->mask == 0)
    fn(q->r, aux);
  else 
    for (i = 0, m = 1; i < 4; ++i, m <<= 1)
      if ((q->mask & m) && q->leaves[i])
        iterate_qtree(q->leaves[i], fn, aux);
}

static void
fill_map(int r[4], void *aux)
{
  int *p, i, j, x, y, w, h;

  x = (QSIZE * (r[0] - qrect[0]) / qrect[2]);
  y = (QSIZE * (r[1] - qrect[1]) / qrect[3]);
  w = (QSIZE * r[2] / qrect[2]);
  h = (QSIZE * r[3] / qrect[3]);
  p = dirty_map + x + y * QSIZE;
  for (j = 0; j < h; ++j) {
    for (i = 0; i < w; ++i, ++p) {
      if (*p)
        fprintf(stderr, "overlap\n");
      *p = 1;
    }
    p += QSIZE - w;
  }
}

static void
fill_dirty_row(int row)
{
  int i, j, start = -1, *p = dirty_map, ngaps;

  for (j = 0; j < QSIZE; ++j) {
    ngaps = 0;
    for (i = 0; i < QSIZE; ++i, ++p) {
      if (!*p && start < 0) {
        start = i;
      } else if (*p && start >= 0) {
        dirty_row[start] = i - start + 1;
        ++ngaps;
        start = -1;
      }
    }
  }
}

static void
opt_qtree_aux(struct qleaf *q)
{
  int i, n = 0;
  struct qleaf **t;

  for (t = q->leaves, i = 0; i < 4; ++i, ++t)
    if (*t) {
      opt_qtree_aux(*t);
      if ((*t)->mask == 0)
        ++n;
    }
  q->mask = (n == 4) ? 0 : q->mask;
}

static void
prep_dirty_rect(int r[4], void *aux)
{
  memcpy(dirty_rects + (ndirty_rects++ << 2), r, sizeof(int[4]));
}

void
prepare_dirty_rects(void)
{
  ndirty_rects = 0;
  if (draw_qtree) {
    print_qtree_leaf(draw_qtree, 0);
    opt_qtree_aux(draw_qtree);
    print_qtree_leaf(draw_qtree, 0);
    iterate_qtree(draw_qtree, prep_dirty_rect, 0);
  }
}

void
clean_dirty_rects(void)
{
  qused = 0;
  draw_qtree = 0;
  ndirty_rects = 0;
}

#if 0
#include <stdarg.h>

void
log_printf(int mask, char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
}

int
main(void)
{
  int *d, i;
  init_dirty(0, 0, 320, 240);
  if (0) add_dirty_rect2(10, 10, 50, 50);
  if (1) add_dirty_rect2(150, 100, 50, 50);
  prepare_dirty_rects();
  fprintf(stderr, "nrects: %d\n", ndirty_rects);
  for (i = 0, d = dirty_rects; i < ndirty_rects; ++i, d += 4)
    fprintf(stderr, "  %d: [%d %d %d %d]\n", i, d[0], d[1], d[2], d[3]);
  return 0;
}
#endif
