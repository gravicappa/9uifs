#include <string.h>
#include <stdlib.h>

#include "util.h"
#include "9p.h"
#include "fs.h"
#include "fstypes.h"
#include "ctl.h"

struct ctl_context {
  struct arr *buf;
};

struct ctl_file {
  struct file f;
  struct ctl_cmd *cmd;
  void *aux;
};

static void
exec_cmd(int cmdlen, char *cmd, struct ctl_cmd *c, void *aux)
{
  char *p = cmd, *s;
  int i;

  s = next_arg(&p);
  for (i = 0; c[i].name && c[i].name[0] && strcmp(c[i].name, s); ++i) {}
  if (c[i].fn)
    c[i].fn(p, aux);
}

static void
ctl_rm(struct p9_fid *fid)
{
  struct ctl_context *ctx;

  fid->rm = 0;
  ctx = (struct ctl_context *)fid->aux;
  if (ctx) {
      return;
    if (ctx->buf)
      free(ctx->buf);
    free(ctx);
  }
  fid->aux = 0;
}

static void
ctl_open(struct p9_connection *con)
{
  struct ctl_context *ctx;

  ctx = (struct ctl_context *)calloc(1, sizeof(struct ctl_context));
  if (!ctx) {
    P9_SET_STR(con->r.ename, "out of memory");
    return;
  }
  con->t.pfid->aux = ctx;
  con->t.pfid->rm = ctl_rm;
}

static void
ctl_write(struct p9_connection *con)
{
  struct ctl_context *ctx;
  char *s, *cmd;
  struct ctl_file *ctl = (struct ctl_file *)con->t.pfid->file;
  int n, i, cmdlen;

  ctx = (struct ctl_context *)con->t.pfid->aux;
  if (!ctx)
    return;
  i = 0;
  s = con->t.data;
  n = con->t.count;
  con->r.count = con->t.count;
  cmd = s;
  while (i < n) {
    for (;i < n && *s && *s != '\n' && *s != '\r'; ++s, ++i) {}
    cmdlen = s - cmd;
    if (ctx->buf && ctx->buf->used) {
      if (arr_memcpy(&ctx->buf, 256, -1, i, con->t.data) < 0) {
        P9_SET_STR(con->r.ename, "out of memory");
        return;
      }
      cmd = ctx->buf->b;
      cmdlen = ctx->buf->used;
      ctx->buf->used = 0;
    }
    if (i >= n)
      break;
    *s++ = 0;
    i++;
    exec_cmd(cmdlen, cmd, ctl->cmd, ctl->aux);
    if (ctx->buf)
      arr_delete(&ctx->buf, 0, i);
    cmd = s;
  }
  if (ctx && cmd == ctx->buf->b) {
    ctx->buf->used = 0;
    return;
  }
  if ((s - cmd) && (arr_memcpy(&ctx->buf, 256, -1, s - cmd, cmd) < 0)) {
    P9_SET_STR(con->r.ename, "out of memory");
    return;
  }
}

static void
ctl_clunk(struct p9_connection *con)
{
  struct ctl_context *ctx;
  struct ctl_file *ctl = (struct ctl_file *)con->t.pfid->file;

  ctx = (struct ctl_context *)con->t.pfid->aux;

  if (ctx && ctx->buf && ctx->buf->used) {
    exec_cmd(ctx->buf->used, ctx->buf->b, ctl->cmd, ctl->aux);
    ctx->buf->used = 0;
  }
}

static void
rm_ctl(struct file *f)
{
  free(f);
}

static struct p9_fs ctl_fs = {
  .open = ctl_open,
  .write = ctl_write,
  .clunk = ctl_clunk
};

struct file *
mk_ctl(char *name, struct ctl_cmd *cmd, void *aux)
{
  struct ctl_file *f;
  f = calloc(1, sizeof(struct ctl_file));
  if (f) {
    f->f.name = name;
    f->f.mode = 0200 | P9_DMAPPEND;
    f->f.qpath = new_qid(FS_CTL);
    f->f.fs = &ctl_fs;
    f->cmd = cmd;
    f->aux = aux;
    f->f.rm = rm_ctl;
  }
  return &f->f;
}
