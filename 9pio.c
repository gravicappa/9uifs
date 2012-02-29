#include "9p.h"
#include "util.h"

int
p9_process_srv(int in_size, char *in, int out_size, char *out, 
               struct p9_connection *c, struct p9_fs *fs)
{
  log_printf(3, ";   unpacking msg\n");
  if (p9_unpack_msg(in_size, in, &c->t))
    return -1;
  log_printf(3, ";   c->t.type: %d\n", c->t.type);
  if (p9_process_treq(c, fs))
    return -1;
  log_printf(3, ";   packing msg %d\n", c->r.type);
  if (p9_pack_msg(out_size, out, &c->r))
    return -1;
  return 0;
}
