#include <stdlib.h>
#include <string.h>

#include "9p.h"
#include "fs.h"
#include "fstypes.h"
#include "fsutil.h"
#include "draw.h"

const char *
font_file(const char *name, const char *style)
{
  return name;
}

int
init_fonts()
{
  return 0;
}

void
free_fonts()
{
}

static void
rm_fonts(struct file *f)
{
  free(f);
}

static void
fid_free(struct p9_fid *fid)
{
  if (fid->aux)
    free(fid->aux);
  fid->rm = 0;
}

static void
open_list(struct p9_connection *c)
{
  int i, n, off = 0, size, *sizes, mode = c->t.pfid->open_mode;
  const char **list;
  unsigned char *buf;

  if ((mode & 3) == P9_OWRITE || (mode & 3) == P9_ORDWR) {
    P9_SET_STR(c->r.ename, "Operation not permitted");
    return;
  }
  c->t.pfid->aux = 0;
  c->t.pfid->rm = fid_free;
  list = font_list(&n);
  if (n == 0 || list == 0)
    return;
  sizes = malloc(n * sizeof(int));
  if (!sizes) {
    P9_SET_STR(c->r.ename, "out of memory");
    return;
  }
  size = 1;
  for (i = 0; i < n; ++i) {
    sizes[i] = strlen(list[i]);
    size += sizes[i] + 1;
  }
  buf = malloc(size + 4);
  if (!buf) {
    free(sizes);
    P9_SET_STR(c->r.ename, "out of memory");
    return;
  }
  buf[off++] = size & 0xff;
  buf[off++] = (size >> 8) & 0xff;
  buf[off++] = (size >> 16) & 0xff;
  buf[off++] = (size >> 24) & 0xff;
  for (i = 0; i < n; ++i) {
    memcpy(buf + off, list[i], sizes[i]);
    off += sizes[i];
    buf[off++] = '\n';
  }
  buf[off] = 0;
  c->t.pfid->aux = buf;
  free(sizes);
}

static void
read_list(struct p9_connection *c)
{
  int size;
  unsigned char *buf;

  buf = c->t.pfid->aux;
  if (!buf)
    return;
  size = (buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24));
  read_data_fn(c, size, (char *)buf + 4);
}

static struct p9_fs list_fs = {
  .open = open_list,
  .read = read_list
};

int
init_fonts_fs(struct file *fs)
{
  struct file *f;

  f = calloc(2, sizeof(struct file));
  if (!f)
    return -1;
  fs->mode = 0700 | P9_DMDIR;
  fs->qpath = new_qid(FS_FONTS);

  f[0].name = "list";
  f[0].mode = 0400;
  f[0].qpath = new_qid(FS_FNT_LIST);
  f[0].fs = &list_fs;
  f[0].rm = rm_fonts;
  add_file(fs, &f[0]);

  return 0;
}
