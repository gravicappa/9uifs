#include <stdio.h>
#include "9p.h"

#define OUT_UINT(m, name) \
  fprintf(stderr, ";       " # name ": %u\n", m->name)

#define OUT_UINT8(m, name) \
  fprintf(stderr, ";       " # name ": %llu\n", m->name)

#define OUT_STR1(m, len, name) \
  fprintf(stderr, ";       " # name ": '%.*s'\n", m->len, m->name)

#define OUT_STR(m, name) OUT_STR1(m, name##_len, name)

static void
print_data_hex(const char *name, int len, char *data)
{
  fprintf(stderr, ";       %s_ptr: %p\n", name, data);
  fprintf(stderr, ";       %s:", name);
  for (; len > 0; --len, ++data)
    fprintf(stderr, " %02x", (unsigned char)*data);
  fprintf(stderr, "\n");
}

static void
pprint_msg_hdr(struct p9_msg *m, const char *type, char *dir)
{
  fprintf(stderr, "\n;  %s MSG\n", dir);
  OUT_UINT(m, size);
  fprintf(stderr, ";       type: %s\n", type);
  OUT_UINT(m, tag);
}

void
p9_print_msg(struct p9_msg *m, char *dir)
{
  int i;

  switch (m->type) {
  case P9_TVERSION:
    pprint_msg_hdr(m, "Tversion", dir);
    OUT_UINT(m, msize);
    OUT_STR(m, version);
    break;

  case P9_RVERSION:
    pprint_msg_hdr(m, "Rversion", dir);
    OUT_UINT(m, msize);
    OUT_STR(m, version);
    break;

  case P9_TAUTH:
    pprint_msg_hdr(m, "Tauth", dir);
    OUT_UINT(m, afid);
    OUT_STR(m, uname);
    OUT_STR(m, aname);
    break;

  case P9_RAUTH:
    pprint_msg_hdr(m, "Rauth", dir);
    OUT_UINT(m, qid.type);
    OUT_UINT(m, qid.version);
    OUT_UINT8(m, qid.path);
    break;

  case P9_TATTACH:
    pprint_msg_hdr(m, "Tattach", dir);
    OUT_UINT(m, fid);
    OUT_UINT(m, afid);
    OUT_STR(m, uname);
    OUT_STR(m, aname);
    break;

  case P9_RATTACH:
    pprint_msg_hdr(m, "Rattach", dir);
    OUT_UINT(m, qid.type);
    OUT_UINT(m, qid.version);
    OUT_UINT8(m, qid.path);
    break;

  case P9_TERROR:
    pprint_msg_hdr(m, "Terror", dir);
    break;

  case P9_RERROR:
    pprint_msg_hdr(m, "Rerror", dir);
    OUT_STR(m, ename);
    break;

  case P9_TFLUSH:
    pprint_msg_hdr(m, "Tflush", dir);
    OUT_UINT(m, oldtag);
    break;

  case P9_RFLUSH:
    pprint_msg_hdr(m, "Rflush", dir);
    break;

  case P9_TWALK:
    pprint_msg_hdr(m, "Twalk", dir);
    OUT_UINT(m, fid);
    OUT_UINT(m, newfid);
    OUT_UINT(m, nwname);
    for (i = 0; i < m->nwname; ++i)
      OUT_STR1(m, wname_len[i], wname[i]);
    break;

  case P9_RWALK:
    pprint_msg_hdr(m, "Rwalk", dir);
    OUT_UINT(m, nwqid);
    for (i = 0; i< m->nwqid; ++i) {
      OUT_UINT(m, wqid[i].type);
      OUT_UINT(m, wqid[i].version);
      OUT_UINT8(m, wqid[i].path);
    }
    break;

  case P9_TOPEN:
    pprint_msg_hdr(m, "Topen", dir);
    OUT_UINT(m, fid);
    OUT_UINT(m, mode);
    break;

  case P9_ROPEN:
    pprint_msg_hdr(m, "Ropen", dir);
    OUT_UINT(m, qid.type);
    OUT_UINT(m, qid.version);
    OUT_UINT8(m, qid.path);
    OUT_UINT(m, iounit);
    break;

  case P9_TCREATE:
    pprint_msg_hdr(m, "Tcreate", dir);
    OUT_UINT(m, fid);
    OUT_STR(m, name);
    OUT_UINT(m, perm);
    OUT_UINT(m, mode);
    break;

  case P9_RCREATE:
    pprint_msg_hdr(m, "Rcreate", dir);
    OUT_UINT(m, qid.type);
    OUT_UINT(m, qid.version);
    OUT_UINT8(m, qid.path);
    OUT_UINT(m, iounit);
    break;

  case P9_TREAD:
    pprint_msg_hdr(m, "tread", dir);
    OUT_UINT(m, fid);
    OUT_UINT8(m, offset);
    OUT_UINT(m, count);
    break;

  case P9_RREAD:
    pprint_msg_hdr(m, "rread", dir);
    OUT_UINT(m, count);
    print_data_hex("data", m->count, m->data);
    break;

  case P9_TWRITE:
    pprint_msg_hdr(m, "Twrite", dir);
    OUT_UINT(m, fid);
    OUT_UINT8(m, offset);
    OUT_UINT(m, count);
    print_data_hex("data", m->count, m->data);
    break;

  case P9_RWRITE:
    pprint_msg_hdr(m, "Rwrite", dir);
    OUT_UINT(m, count);
    break;

  case P9_TCLUNK:
    pprint_msg_hdr(m, "Tclunk", dir);
    OUT_UINT(m, fid);
    break;

  case P9_RCLUNK:
    pprint_msg_hdr(m, "Rclunk", dir);
    break;

  case P9_TREMOVE:
    pprint_msg_hdr(m, "Tremove", dir);
    OUT_UINT(m, fid);
    break;

  case P9_RREMOVE:
    pprint_msg_hdr(m, "Rremove", dir);
    break;

  case P9_TSTAT:
    pprint_msg_hdr(m, "Tstat", dir);
    OUT_UINT(m, fid);
    break;

  case P9_RSTAT:
    pprint_msg_hdr(m, "Rstat", dir);
    break;

  case P9_TWSTAT:
    pprint_msg_hdr(m, "Twstat", dir);
    OUT_UINT(m, fid);
    break;

  case P9_RWSTAT:
    pprint_msg_hdr(m, "Rwstat", dir);
    break;

  default:
    fprintf(stderr, ";  %s UNKNOWN MSG %u\n", dir, m->type);
  }
  fprintf(stderr, "\n\n");
}
