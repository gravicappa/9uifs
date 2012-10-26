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

  log_printf(LOG_DBG, "exec_cmd %d '%s'\n", n, str);
  s = next_arg(&p);
  for (i = 0; f->cmd[i].name && strcmp(f->cmd[i].name, s); ++i) {}
  if (f->cmd[i].fn)
    f->cmd[i].fn(&f->f, p);
}

static void
ctl_rm(struct p9_fid *fid)
{
  struct ctl_context *ctx;
  struct ctl_file *f;

  fid->rm = 0;
  f = (struct ctl_file *)fid->file;
  ctx = (struct ctl_context *)fid->aux;
  if (ctx) {
      return;
    if (ctx->buf)
      free(ctx->buf);
    free(ctx);
  }
  fid->aux = 0;
}

void
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

void
ctl_write(struct p9_connection *con)
{
  struct ctl_context *ctx;
  struct ctl_file *f;
  char *s, *cmd;
  int n, i, cmdlen;

  f = (struct ctl_file *)con->t.pfid->file;
  ctx = (struct ctl_context *)con->t.pfid->aux;
  if (!ctx)
    return;
  i = 0;
  s = con->t.data;
  n = con->t.count;
  con->r.count = con->t.count;
  if (0) log_printf(LOG_DBG, "ctl_write  buf: '%.*s'\n", n, s);
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
      if (0) log_printf(LOG_DBG, "  allocated: '%.*s'\n", cmdlen, cmd);
    }
    if (0) log_printf(LOG_DBG, "  i: %d n: %d\n", i, n);
    if (i >= n)
      break;
    *s++ = 0;
    i++;
    log_printf(LOG_DBG, "  cmd: '%.*s'\n", cmdlen, cmd);
    exec_cmd(f, cmdlen, cmd);
    if (ctx->buf) {
      arr_delete(&ctx->buf, 0, i);
      log_printf(LOG_DBG, "  del buf: %d '%.*s'\n", ctx->buf->used,
                 ctx->buf->used, ctx->buf->b);
    }
    cmd = s;
  }
  if (ctx && cmd == ctx->buf->b) {
    ctx->buf->used = 0;
    return;
  }
  if (0) log_printf(LOG_DBG, "  left: '%.*s'\n", s - cmd, cmd);
  if ((s - cmd) && (arr_memcpy(&ctx->buf, 256, -1, s - cmd, cmd) < 0)) {
    P9_SET_STR(con->r.ename, "out of memory");
    return;
  }
  if (0) log_printf(LOG_DBG, "ctl_write done\n");
}

void
ctl_clunk(struct p9_connection *con)
{
  struct ctl_context *ctx;
  struct ctl_file *f;

  f = (struct ctl_file *)con->t.pfid->file;
  ctx = (struct ctl_context *)con->t.pfid->aux;

  if (ctx && ctx->buf && ctx->buf->used) {
    exec_cmd(f, ctx->buf->used, ctx->buf->b);
    ctx->buf->used = 0;
  }
}

struct p9_fs ctl_fs = {
  .open = ctl_open,
  .write = ctl_write,
  .clunk = ctl_clunk
};
