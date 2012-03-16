#include <string.h>
#include "9p.h"
#include "util.h"

struct p9_stream {
  unsigned int off;
  unsigned int size;
  unsigned char *buf;
};

static void write_uint1(struct p9_stream *s, unsigned char x);
static void write_uint2(struct p9_stream *s, unsigned short x);
static void write_uint4(struct p9_stream *s, unsigned int x);
static void write_uint8(struct p9_stream *s, unsigned long long x);
static void write_str(struct p9_stream *s, int len, char *x);
static void write_data(struct p9_stream *s, int len, char *x);
static void write_qid(struct p9_stream *s, struct p9_qid *qid);
static void write_stat(struct p9_stream *s, struct p9_stat *stat);

static unsigned char read_uint1(struct p9_stream *s, int *err);
static unsigned short read_uint2(struct p9_stream *s, int *err);
static unsigned int read_uint4(struct p9_stream *s, int *err);
static unsigned long long read_uint8(struct p9_stream *s, int *err);
static char *read_str(struct p9_stream *s, unsigned int *len, int *err);
static void read_qid(struct p9_stream *s, struct p9_qid *qid, int *err);
static char *read_data(struct p9_stream *s, unsigned int *len, int *err);
static void read_stat(struct p9_stream *s, struct p9_stat *stat, int *err);

int
p9_unpack_msg(int bytes, char *buf, struct p9_msg *m)
{
  struct p9_stream s = {0, 0, 0};
  unsigned int i;
  int err = 0;

  s.buf = (unsigned char *)buf;
  s.size = bytes;

  s.size = read_uint4(&s, &err);
  m->type = read_uint1(&s, &err);
  m->tag = read_uint2(&s, &err);

  switch (m->type) {
    case P9_TVERSION:
    case P9_RVERSION:
      m->msize = read_uint4(&s, &err);
      m->version = read_str(&s, &m->version_len, &err);
      break;

    case P9_TAUTH:
      m->afid = read_uint4(&s, &err);
      m->uname = read_str(&s, &m->uname_len, &err);
      m->aname = read_str(&s, &m->aname_len, &err);
      break;

    case P9_RAUTH:
    case P9_RATTACH:
      read_qid(&s, &m->aqid, &err);
      break;

    case P9_RERROR:
      m->ename = read_str(&s, &m->ename_len, &err);
      break;

    case P9_TFLUSH:
      m->oldtag = read_uint2(&s, &err);
      break;

    case P9_RFLUSH:
    case P9_RCLUNK:
    case P9_RREMOVE:
    case P9_RWSTAT:
      break;

    case P9_TATTACH:
      m->fid = read_uint4(&s, &err);
      m->afid = read_uint4(&s, &err);
      m->uname = read_str(&s, &m->uname_len, &err);
      m->aname = read_str(&s, &m->aname_len, &err);
      break;

    case P9_TWALK:
      m->fid = read_uint4(&s, &err);
      m->newfid = read_uint4(&s, &err);
      m->nwname = read_uint2(&s, &err);
      for (i = 0; i < m->nwname && i < P9_MAXWELEM; ++i)
        m->wname[i] = read_str(&s, &m->wname_len[i], &err);
      break;

    case P9_RWALK:
      m->nwqid = read_uint2(&s, &err);
      for (i = 0; i < m->nwqid && i < P9_MAXWELEM; ++i)
        read_qid(&s, &m->wqid[i], &err);
      break;

    case P9_TOPEN:
      m->fid = read_uint4(&s, &err);
      m->mode = read_uint1(&s, &err);
      break;

    case P9_ROPEN:
    case P9_RCREATE:
      read_qid(&s, &m->qid, &err);
      m->iounit = read_uint4(&s, &err);
      break;

    case P9_TCREATE:
      m->fid = read_uint4(&s, &err);
      m->name = read_str(&s, &m->name_len, &err);
      m->perm = read_uint4(&s, &err);
      m->mode = read_uint1(&s, &err);
      break;

    case P9_TREAD:
      m->fid = read_uint4(&s, &err);
      m->offset = read_uint8(&s, &err);
      m->count = read_uint4(&s, &err);
      break;

    case P9_RREAD:
      m->data = read_data(&s, &m->count, &err);
      break;

    case P9_TWRITE:
      m->fid = read_uint4(&s, &err);
      m->offset = read_uint8(&s, &err);
      m->data = read_data(&s, &m->count, &err);
      break;

    case P9_RWRITE:
      m->count = read_uint4(&s, &err);
      break;

    case P9_TCLUNK:
    case P9_TREMOVE:
    case P9_TSTAT:
      m->fid = read_uint4(&s, &err);
      break;

    case P9_RSTAT:
      i = read_uint2(&s, &err);
      read_stat(&s, &m->stat, &err);
      break;

    case P9_TWSTAT:
      m->fid = read_uint4(&s, &err);
      i = read_uint2(&s, &err);
      read_stat(&s, &m->stat, &err);
      break;
    default: err = -1;
  }
  return err;
}

int
p9_pack_msg(int bytes, char *buf, struct p9_msg *m)
{
  struct p9_stream s = {4, 0, 0};
  unsigned int size, i;

  if (bytes < 7)
    return -1;

  s.buf = (unsigned char *)buf;
  s.size = bytes;

  write_uint1(&s, m->type);
  write_uint2(&s, m->tag);
  switch (m->type) {
    case P9_TVERSION:
    case P9_RVERSION:
      if (s.off + 4 + 2 + m->version_len >= s.size)
        return -1;
      write_uint4(&s, m->msize); 
      write_str(&s, m->version_len, m->version);
      break;

    case P9_TAUTH:
      if (s.off + 4 + 2 + m->uname_len + 2 + m->aname_len >= s.size)
        return -1;
      write_uint4(&s, m->afid);
      write_str(&s, m->uname_len, m->uname);
      write_str(&s, m->aname_len, m->aname);
      break;

    case P9_RAUTH:
    case P9_RATTACH:
      if (s.off + 13 >= s.size)
        return -1;
      write_qid(&s, &m->aqid);
      break;

    case P9_RERROR:
      if (s.off + 2 + m->ename_len >= s.size)
        return -1;
      write_str(&s, m->ename_len, m->ename);
      break;

    case P9_TFLUSH:
      if (s.off + 2 >= s.size)
        return -1;
      write_uint2(&s, m->oldtag);
      break;

    case P9_RFLUSH:
    case P9_RCLUNK:
    case P9_RREMOVE:
    case P9_RWSTAT:
      break;

    case P9_TATTACH:
      if (s.off + 4 + 4 + 2 + m->uname_len + 2 + m->aname_len)
        return -1;
      write_uint4(&s, m->fid);
      write_uint4(&s, m->afid);
      write_str(&s, m->uname_len, m->uname);
      write_str(&s, m->aname_len, m->aname);
      break;

    case P9_TWALK:
      size = s.off + 4 + 4 + 2;
      for (i = 0; i < m->nwname && i < P9_MAXWELEM; ++i)
        size += 2 + m->wname_len[i];
      if (size >= s.size)
        return -1;
      write_uint4(&s, m->fid);
      write_uint4(&s, m->newfid);
      write_uint2(&s, m->nwname);
      for (i = 0; i < m->nwname && i < P9_MAXWELEM; ++i)
        write_str(&s, m->wname_len[i], m->wname[i]);
      break;

    case P9_RWALK:
      if (s.off + 2 + 13 * m->nwqid >= s.size)
        return -1;
      write_uint2(&s, m->nwqid);
      for (i = 0; i < m->nwqid && i < P9_MAXWELEM; ++i)
        write_qid(&s, &m->wqid[i]);
      break;

    case P9_TOPEN:
      if (s.off + 4 + 1 >= s.size)
        return -1;
      write_uint4(&s, m->fid);
      write_uint1(&s, m->mode);
      break;

    case P9_ROPEN:
    case P9_RCREATE:
      if (s.off + 13 + 4 >= s.size)
        return -1;
      write_qid(&s, &m->qid);
      write_uint4(&s, m->iounit);
      break;

    case P9_TCREATE:
      if (s.off + 4 + 2 + m->name_len + 4 + 1 >= s.size)
        return -1;
      write_uint4(&s, m->fid);
      write_str(&s, m->name_len, m->name);
      write_uint4(&s, m->perm);
      write_uint1(&s, m->mode);
      break;

    case P9_TREAD:
      if (s.off + 4 + 8 + 4 >= s.size)
        return -1;
      write_uint4(&s, m->fid);
      write_uint8(&s, m->offset);
      write_uint4(&s, m->count);
      break;

    case P9_RREAD:
      if (s.off + 4 + m->count >= s.size)
        return -1;
      write_data(&s, m->count, m->data);
      break;

    case P9_TWRITE:
      if (s.off + 4 + 8 + m->count >= s.size)
        return -1;
      write_uint4(&s, m->fid);
      write_uint8(&s, m->offset);
      write_data(&s, m->count, m->data);
      break;

    case P9_RWRITE:
      if (s.off + 4 >= s.size)
        return -1;
      write_uint4(&s, m->count);
      break;

    case P9_TCLUNK:
    case P9_TREMOVE:
    case P9_TSTAT:
      if (s.off + 4 >= s.size)
        return -1;
      write_uint4(&s, m->fid);
      break;

    case P9_RSTAT:
      m->stat.size = p9_stat_size(&m->stat);
      if (s.off + 2 + 2 + m->stat.size >= s.size)
        return -1;
      write_uint2(&s, m->stat.size + 2);
      write_stat(&s, &m->stat);
      break;

    case P9_TWSTAT:
      m->stat.size = p9_stat_size(&m->stat);
      if (s.off + 4 + 2 + 2 + m->stat.size >= s.size)
        return -1;
      write_uint4(&s, m->fid);
      write_uint2(&s, m->stat.size + 2);
      write_stat(&s, &m->stat);
      break;

    default: return -1;
  }
  s.buf[0] = s.off & 0xff;
  s.buf[1] = (s.off >> 8) & 0xff;
  s.buf[2] = (s.off >> 16) & 0xff;
  s.buf[3] = (s.off >> 24) & 0xff;
  return 0;
}

static void
write_uint1(struct p9_stream *s, unsigned char x)
{
  s->buf[s->off++] = x;
}

static void
write_uint2(struct p9_stream *s, unsigned short x)
{
  s->buf[s->off++] = x & 0xff;
  s->buf[s->off++] = (x >> 8) & 0xff;
}

static void
write_uint4(struct p9_stream *s, unsigned int x)
{
  s->buf[s->off++] = x & 0xff;
  s->buf[s->off++] = (x >> 8) & 0xff;
  s->buf[s->off++] = (x >> 16) & 0xff;
  s->buf[s->off++] = (x >> 24) & 0xff;
}

static void
write_uint8(struct p9_stream *s, unsigned long long x)
{
  s->buf[s->off++] = x & 0xff;
  s->buf[s->off++] = (x >> 8) & 0xff;
  s->buf[s->off++] = (x >> 16) & 0xff;
  s->buf[s->off++] = (x >> 24) & 0xff;
  s->buf[s->off++] = (x >> 32) & 0xff;
  s->buf[s->off++] = (x >> 40) & 0xff;
  s->buf[s->off++] = (x >> 48) & 0xff;
  s->buf[s->off++] = (x >> 56) & 0xff;
}

static void
write_str(struct p9_stream *s, int len, char *x)
{
  write_uint2(s, len);
  memcpy(s->buf + s->off, x, len);
  s->off += len;
}

static void
write_data(struct p9_stream *s, int len, char *x)
{
  write_uint4(s, len);
  memcpy(s->buf + s->off, x, len);
  s->off += len;
}

static void
write_qid(struct p9_stream *s, struct p9_qid *qid)
{
  write_uint1(s, qid->type);
  write_uint4(s, qid->version);
  write_uint8(s, qid->path);
}

static void
write_stat(struct p9_stream *s, struct p9_stat *stat)
{
  p9_pack_stat(s->size - s->off, (char *)s->buf + s->off, stat);
  s->off += stat->size + 2;
}

static unsigned char
read_uint1(struct p9_stream *s, int *err)
{
  if (*err |= s->off + 1 > s->size)
    return 0;
  return s->buf[s->off++];
}

static unsigned short
read_uint2(struct p9_stream *s, int *err)
{
  unsigned short x;

  if (*err |= (s->off + 2 > s->size))
    return 0;
  x = s->buf[s->off] | (s->buf[s->off + 1] << 8);
  s->off += 2;
  return x;
}

static unsigned int
read_uint4(struct p9_stream *s, int *err)
{
  unsigned int x;

  if (*err |= (s->off + 4 > s->size))
    return 0;

  x = (s->buf[s->off]
       | (s->buf[s->off + 1] << 8)
       | (s->buf[s->off + 2] << 16)
       | (s->buf[s->off + 3] << 24));
  s->off += 4;
  return x;
}

static unsigned long long
read_uint8(struct p9_stream *s, int *err)
{
  unsigned long long x;

  if (*err |= (s->off + 8 > s->size))
    return 0;

  x = read_uint4(s, err);
  if (*err)
    return 0;
  x |= ((unsigned long long)read_uint4(s, err)) << 32;
  return x;
}

static char *
read_str(struct p9_stream *s, unsigned int *len, int *err)
{
  char *x;

  if (*err |= (s->off + 2 > s->size))
    return 0;

  *len = read_uint2(s, err);
  if (*err |= (s->off + *len > s->size))
    return 0;
  if (!*len)
    return 0;
  x = (char *)s->buf + s->off;
  s->off += *len;
  return x;
}

static void
read_qid(struct p9_stream *s, struct p9_qid *qid, int *err)
{
  if (*err |= (s->off + 13 > s->size))
    return;
  qid->type = read_uint1(s, err);
  qid->version = read_uint4(s, err);
  qid->path = read_uint8(s, err);
}

static char *
read_data(struct p9_stream *s, unsigned int *len, int *err)
{
  char *x;

  if (*err |= (s->off + 4 > s->size))
    return 0;

  *len = read_uint4(s, err);
  if (*err |= (s->off + *len > s->size))
    return 0;
  x = (char *)s->buf + s->off;
  s->off += *len;
  return x;
}

static void
read_stat(struct p9_stream *s, struct p9_stat *stat, int *err)
{
  *err |= p9_unpack_stat(s->size - s->off, (char *)s->buf + s->off, stat);
  s->off += s->size;
}

int
p9_stat_size(struct p9_stat *stat)
{
  return 2 + 4 + 13 + 4 + 4 + 4 + 8 + 2 + stat->name_len + 2 + stat->uid_len
      + 2 + stat->gid_len + 2 + stat->muid_len;
}

int
p9_pack_stat(int bytes, char *buf, struct p9_stat *stat)
{
  struct p9_stream s = { 0, 0, 0 };

  s.size = bytes;
  s.buf = (unsigned char *)buf;

  if (s.off + 2 + stat->size >= s.size)
    return 1;

  write_uint2(&s, stat->size);
  write_uint2(&s, stat->type);
  write_uint4(&s, stat->dev);
  write_qid(&s, &stat->qid);
  write_uint4(&s, stat->mode);
  write_uint4(&s, stat->atime);
  write_uint4(&s, stat->mtime);
  write_uint8(&s, stat->length);
  write_str(&s, stat->name_len, stat->name);
  write_str(&s, stat->uid_len, stat->uid);
  write_str(&s, stat->gid_len, stat->gid);
  write_str(&s, stat->muid_len, stat->muid);
  return 0;
}

int
p9_unpack_stat(int bytes, char *buf, struct p9_stat *stat)
{
  struct p9_stream s = {0, 0, 0};
  int err = 0;

  s.size = bytes;
  s.buf = (unsigned char *)buf;

  stat->size = read_uint2(&s, &err);
  stat->type = read_uint2(&s, &err);
  stat->dev = read_uint4(&s, &err);
  read_qid(&s, &stat->qid, &err);
  stat->mode = read_uint4(&s, &err);
  stat->atime = read_uint4(&s, &err);
  stat->mtime = read_uint4(&s, &err);
  stat->length = read_uint8(&s, &err);
  stat->name = read_str(&s, &stat->name_len, &err);
  stat->uid = read_str(&s, &stat->uid_len, &err);
  stat->gid = read_str(&s, &stat->gid_len, &err);
  stat->muid = read_str(&s, &stat->muid_len, &err);
  return err;
}

int
p9_process_treq(struct p9_connection *c, struct p9_fs *fs)
{
  int (*fn)(struct p9_connection *c) = 0;

  switch (c->t.type) {
  case P9_TVERSION: fn = fs->version; break;
  case P9_TAUTH: fn = fs->auth; break;
  case P9_TATTACH: fn = fs->attach; break;
  case P9_TFLUSH: fn = fs->flush; break;
  case P9_TWALK: fn = fs->walk; break;
  case P9_TOPEN: fn = fs->open; break;
  case P9_TCREATE: fn = fs->create; break;
  case P9_TREAD: fn = fs->read; break;
  case P9_TWRITE: fn = fs->write; break;
  case P9_TCLUNK: fn = fs->clunk; break;
  case P9_TREMOVE: fn = fs->remove; break;
  case P9_TSTAT: fn = fs->stat; break;
  case P9_TWSTAT: fn = fs->wstat; break;
  }
  memset(&c->r, 0, sizeof(c->r));
  c->r.type = P9_RERROR;
  c->r.tag = c->t.tag;
  if (fn)
    fn(c);
  else
    P9_SET_STR(c->r.ename, "Function not implemented");
  if (!c->r.ename)
    c->r.type = c->t.type ^ 1;
  return 0;
}
