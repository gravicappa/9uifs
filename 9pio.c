#include "9p.h"
#include "util.h"

int
p9_process_srv(int in_size, char *in, int out_size, char *out, 
               struct p9_connection *c, struct p9_fs *fs)
{
  if (p9_unpack_msg(in_size, in, &c->t))
    return -1;
  c->r.deferred = 0;
  if(p9_process_treq(c, fs))
    return -1;
  if (c->r.deferred)
    return 0;
  if (p9_pack_msg(out_size, out, &c->r))
    return -1;
  return 0;
}

void
p9_walk(struct p9_connection *c, struct p9_fs *fs)
{
  char *name;

  if (c->r.nwqid >= c->t.nwname) {
    /* DONE */
    return;
  }
  if (!fs->walk1) {
    /* ERR */
  }

  fs->walk1(c, )
}
