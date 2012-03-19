#include "9p.h"
#include "9pdbg.h"

int
p9_process_msg(int in_size, char *in, struct p9_connection *c,
               struct p9_fs *fs)
{
  if (p9_unpack_msg(in_size, in, &c->t))
    return -1;
  p9_print_msg(&c->t, ">>");
  c->r.deferred = 0;
  if(p9_process_treq(c, fs))
    return -1;
  if (c->r.deferred)
    return 0;
  p9_print_msg(&c->r, "<<");
  if (p9_pack_msg(out_size, out, &c->r))
    return -1;
  return 0;
}
