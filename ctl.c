#include <string.h>
#include <stdlib.h>

#include "util.h"
#include "9p.h"
#include "fs.h"
#include "ctl.h"

void
exec_cmd(struct ctl_file *f, int n, char *str)
{
  char *p = str, *s;
  int i;

  s = next_arg(&p);
  for (i = 0; f->cmd[i].name && strcmp(f->cmd[i].name, s); ++i) {}
  if (f->cmd[i].fn)
    f->cmd[i].fn(&f->file, p);
}

static void
ctl_rm(struct p9_fid *fid)
{
  struct ctl_context *cc;
  struct ctl_file *f;

  fid->rm = 0;
  f = (struct ctl_file *)fid->file;
  cc = (struct ctl_context *)fid->aux;
  if (!cc)
    return;

  if (cc->buf.used)
    exec_cmd(f, cc->buf.used, (char *)cc->buf.b);
  free(cc->buf.b);
  free(cc);
  fid->aux = 0;
}

void
ctl_open(struct p9_connection *c)
{
  struct ctl_context *cc;

  cc = (struct ctl_context *)malloc(sizeof(struct ctl_context));
  if (!cc) {
    P9_SET_STR(c->r.ename, "out of memory");
    return;
  }
  memset(cc, 0, sizeof(*cc));
  cc->buf.delta = 256;
  c->t.pfid->aux = cc;
  c->t.pfid->rm = ctl_rm;
}

void
ctl_write(struct p9_connection *c)
{
  struct ctl_context *cc;
  struct ctl_file *f;
  char *p;
  int n;

  f = (struct ctl_file *)c->t.pfid->file;
  cc = (struct ctl_context *)c->t.pfid->aux;
  if (!cc)
    return;
  p = strnchr(c->t.data, c->t.count, '\n');
  add_data(&cc->buf, c->t.count, c->t.data);
  c->r.count = c->t.count;
  if (!p)
    return;
  n = cc->buf.used + ((char *)c->t.data - p);
  cc->buf.b[n] = 0;
  exec_cmd(f, n, (char *)cc->buf.b);
  rm_data(&cc->buf, n, cc->buf.b);
}

struct p9_fs ctl_fs = {
  .open = ctl_open,
  .write = ctl_write
};
