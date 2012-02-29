#include <string.h>
#include "9p.h"
#include "client.h"

static void
version(struct p9_connection *c)
{
  struct client *cl = (struct client *)c;

  if (strncmp(P9_VERSION, c->t.version, c->t.version_len)) {
    c->t.ename = "Protocol not supported";
    c->t.ename_len = strlen(c->t.ename);
  }
  log_printf(5, "; msize: %d\n", c->t.msize);
  if (cl->msize > c->t.msize)
    cl->msize = c->t.msize;
  log_printf(5, "; msize: %d\n", cl->msize);
  c->r.msize = cl->msize;
  c->r.version = P9_VERSION;
  c->r.version_len = strlen(c->r.version);
}

struct p9_fs fs = {
  .version = version,
  .auth = 0,
  .attach = 0,
  .flush = 0,
  .walk = 0,
  .open = 0,
  .create = 0,
  .read = 0,
  .write = 0,
  .clunk = 0,
  .remove = 0,
  .stat = 0,
  .wstat = 0
};
