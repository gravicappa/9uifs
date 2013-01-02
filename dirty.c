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

static struct qleaf leaves[NLEAVES(QDEPTH)] = {{0}};

static void
print_qtree_leaf(int level, struct qleaf *q)
{
  int i = level;

  while (i--)
    log_printf(LOG_DBG, "  ");
  log_printf(LOG_DBG, "%d%d%d%d [%d %d %d %d]\n",
             !!(q->mask & 8), !!(q->mask & 4), !!(q->mask & 2), q->mask & 1,
             q->r[0], q->r[1], q->r[2], q->r[3]);
  for (i = 0; i < 4; ++i)
    if (q->leaves[i])
      print_qtree_leaf(level + 1, q->leaves[i]);
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
mark_dirty_rect(int r[4])
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

static void
iter_dirty_aux(struct qleaf *q, void (*fn)(int r[4], void *aux), void *aux)
{
  int i, m;

  if (q->mask == 0) {
    fn(q->r, aux);
    return;
  }
  for (i = 0, m = 1; i < 4; ++i, m <<= 1)
    if ((q->mask & m) && q->leaves[i])
      iter_dirty_aux(q->leaves[i], fn, aux);
}

static void
opt_dirty_aux(struct qleaf *q)
{
  int i, n = 0;
  struct qleaf **t;

  for (t = q->leaves, i = 0; i < 4; ++i, ++t)
    if (*t) {
      opt_dirty_aux(*t);
      if ((*t)->mask == 0)
        ++n;
    }
  q->mask = (n == 4) ? 0 : q->mask;
}

void
optimize_dirty_rects()
{
  if (draw_qtree)
    opt_dirty_aux(draw_qtree);
}

void
iterate_dirty_rects(void (*fn)(int r[4], void *aux), void *aux)
{
  if (draw_qtree) {
    optimize_dirty_rects();
    iter_dirty_aux(draw_qtree,  fn, aux);
  }
}

void
clean_dirty_rects()
{
  qused = 0;
  draw_qtree = 0;
}
