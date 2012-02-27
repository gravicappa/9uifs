#include <string.h>
#include "9p.h"

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
p9_read_msg(int bytes, unsigned char *buf, struct p9_msg *m)
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
    case P9_TVersion:
    case P9_RVersion:
      m->msize = read_uint4(&s, &err);
      m->version = read_str(&s, &m->version_len, &err);
      break;

    case P9_TAuth:
      m->afid = read_uint4(&s, &err);
      m->uname = read_str(&s, &m->uname_len, &err);
      m->aname = read_str(&s, &m->aname_len, &err);
      break;

    case P9_RAuth:
    case P9_RAttach:
      read_qid(&s, &m->aqid, &err);
      break;

    case P9_RError:
      m->ename = read_str(&s, &m->ename_len, &err);
      break;

    case P9_TFlush:
      m->oldtag = read_uint2(&s, &err);
      break;

    case P9_RFlush:
    case P9_RClunk:
    case P9_RRemove:
    case P9_RWStat:
      break;

    case P9_TAttach:
      m->fid = read_uint4(&s, &err);
      m->afid = read_uint4(&s, &err);
      m->uname = read_str(&s, &m->uname_len, &err);
      m->aname = read_str(&s, &m->aname_len, &err);
      break;

    case P9_TWalk:
      m->fid = read_uint4(&s, &err);
      m->newfid = read_uint4(&s, &err);
      m->nwname = read_uint2(&s, &err);
      for (i = 0; i < m->nwname && i < P9_MAXWELEM; ++i)
        m->wname[i] = read_str(&s, &m->wname_len[i], &err);
      break;

    case P9_RWalk:
      m->nwqid = read_uint2(&s, &err);
      for (i = 0; i < m->nwqid && i < P9_MAXWELEM; ++i)
        read_qid(&s, &m->wqid[i], &err);
      break;

    case P9_TOpen:
      m->fid = read_uint4(&s, &err);
      m->mode = read_uint1(&s, &err);
      break;

    case P9_ROpen:
    case P9_RCreate:
      read_qid(&s, &m->qid, &err);
      m->iounit = read_uint4(&s, &err);
      break;

    case P9_TCreate:
      m->fid = read_uint4(&s, &err);
      m->name = read_str(&s, &m->name_len, &err);
      m->perm = read_uint4(&s, &err);
      m->mode = read_uint1(&s, &err);
      break;

    case P9_TRead:
      m->fid = read_uint4(&s, &err);
      m->offset = read_uint8(&s, &err);
      m->count = read_uint4(&s, &err);
      break;

    case P9_RRead:
      m->data = read_data(&s, &m->data_len, &err);
      break;

    case P9_TWrite:
      m->fid = read_uint4(&s, &err);
      m->offset = read_uint8(&s, &err);
      m->data = read_data(&s, &m->data_len, &err);
      break;

    case P9_RWrite:
      m->count = read_uint4(&s, &err);
      break;

    case P9_TClunk:
    case P9_TRemove:
    case P9_TStat:
      m->fid = read_uint4(&s, &err);
      break;

    case P9_RStat:
      read_stat(&s, &m->stat, &err);
      break;

    case P9_TWStat:
      m->fid = read_uint4(&s, &err);
      read_stat(&s, &m->stat, &err);
      break;
    default: err = -1;
  }
  return err;
}

int
p9_write_msg(int bytes, char *buf, struct p9_msg *m)
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
    case P9_TVersion:
    case P9_RVersion:
      if (s.off + 4 + 2 + m->version_len >= s.size)
        return -1;
      write_uint4(&s, m->msize); 
      write_str(&s, m->version_len, m->version);
      break;

    case P9_TAuth:
      if (s.off + 4 + 2 + m->uname_len + 2 + m->aname_len >= s.size)
        return -1;
      write_uint4(&s, m->afid);
      write_str(&s, m->uname_len, m->uname);
      write_str(&s, m->aname_len, m->aname);
      break;

    case P9_RAuth:
    case P9_RAttach:
      if (s.off + 13 >= s.size)
        return -1;
      write_qid(&s, &m->aqid);
      break;

    case P9_RError:
      if (s.off + 2 + m->ename_len >= s.size)
        return -1;
      write_str(&s, m->ename_len, m->ename);
      break;

    case P9_TFlush:
      if (s.off + 2 >= s.size)
        return -1;
      write_uint2(&s, m->oldtag);
      break;

    case P9_RFlush:
    case P9_RClunk:
    case P9_RRemove:
    case P9_RWStat:
      break;

    case P9_TAttach:
      if (s.off + 4 + 4 + 2 + m->uname_len + 2 + m->aname_len)
        return -1;
      write_uint4(&s, m->fid);
      write_uint4(&s, m->afid);
      write_str(&s, m->uname_len, m->uname);
      write_str(&s, m->aname_len, m->aname);
      break;

    case P9_TWalk:
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

    case P9_RWalk:
      if (s.off + 2 + 13 * m->nwqid >= s.size)
        return -1;
      write_uint2(&s, m->nwqid);
      for (i = 0; i < m->nwqid && i < P9_MAXWELEM; ++i)
        write_qid(&s, &m->wqid[i]);
      break;

    case P9_TOpen:
      if (s.off + 4 + 1 >= s.size)
        return -1;
      write_uint4(&s, m->fid);
      write_uint1(&s, m->mode);
      break;

    case P9_ROpen:
    case P9_RCreate:
      if (s.off + 13 + 4 >= s.size)
        return -1;
      write_qid(&s, &m->qid);
      write_uint4(&s, m->iounit);
      break;

    case P9_TCreate:
      if (s.off + 4 + 2 + m->name_len + 4 + 1 >= s.size)
        return -1;
      write_uint4(&s, m->fid);
      write_str(&s, m->name_len, m->name);
      write_uint4(&s, m->perm);
      write_uint1(&s, m->mode);
      break;

    case P9_TRead:
      if (s.off + 4 + 8 + 4 >= s.size)
        return -1;
      write_uint4(&s, m->fid);
      write_uint8(&s, m->offset);
      write_uint4(&s, m->count);
      break;

    case P9_RRead:
      if (s.off + 4 + m->data_len >= s.size)
        return -1;
      write_data(&s, m->data_len, m->data);
      break;

    case P9_TWrite:
      if (s.off + 4 + 8 + m->data_len >= s.size)
        return -1;
      write_uint4(&s, m->fid);
      write_uint8(&s, m->offset);
      write_data(&s, m->data_len, m->data);
      break;

    case P9_RWrite:
      if (s.off + 4 >= s.size)
        return -1;
      write_uint4(&s, m->count);
      break;

    case P9_TClunk:
    case P9_TRemove:
    case P9_TStat:
      if (s.off + 4 >= s.size)
        return -1;
      write_uint4(&s, m->fid);
      break;

    case P9_RStat:
      if (s.off + 2 + m->stat.size >= s.size)
        return -1;
      write_stat(&s, &m->stat);
      break;

    case P9_TWStat:
      if (s.off + 4 + 2 + m->stat.size >= s.size)
        return -1;
      write_uint4(&s, m->fid);
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
  memcpy(s->buf, x, len);
  s->off += len;
}

static void
write_data(struct p9_stream *s, int len, char *x)
{
  write_uint4(s, len);
  memcpy(s->buf, x, len);
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
  p9_pack_stat(s->size - s->off, s->buf + s->off, stat);
}

static unsigned char
read_uint1(struct p9_stream *s, int *err)
{
  if (*err |= s->off + 1 >= s->size)
    return 0;
  return s->buf[s->off++];
}

static unsigned short
read_uint2(struct p9_stream *s, int *err)
{
  unsigned short x;

  if (*err |= (s->off + 2 >= s->size))
    return 0;
  x = s->buf[s->off] | (s->buf[s->off + 1] << 8);
  s->off += 2;
  return x;
}

static unsigned int
read_uint4(struct p9_stream *s, int *err)
{
  unsigned int x;

  if (*err |= (s->off + 4 >= s->size))
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

  if (*err |= (s->off + 8 >= s->size))
    return 0;

  x = read_uint4(s, err);
  if (err)
    return 0;
  x |= ((unsigned long long)read_uint4(s, err)) << 32;
  return x;
}

static char *
read_str(struct p9_stream *s, unsigned int *len, int *err)
{
  char *x;

  if (*err |= (s->off + 2 >= s->size))
    return 0;

  *len = read_uint2(s, err);
  if (*err |= (s->off + *len >= s->size))
    return 0;
  x = (char *)s->buf + s->off;
  s->off += *len;
  return x;
}

static void
read_qid(struct p9_stream *s, struct p9_qid *qid, int *err)
{
  if (*err |= (s->off + 13 >= s->size))
    return;
  qid->type = read_uint1(s, err);
  qid->version = read_uint4(s, err);
  qid->path = read_uint8(s, err);
}

static char *
read_data(struct p9_stream *s, unsigned int *len, int *err)
{
  char *x;

  if (*err |= (s->off + 4 >= s->size))
    return 0;

  *len = read_uint4(s, err);
  if (*err |= (s->off + *len >= s->size))
    return 0;
  x = (char *)s->buf + s->off;
  s->off += *len;
  return x;
}

static void
read_stat(struct p9_stream *s, struct p9_stat *stat, int *err)
{
  *err |= p9_unpack_stat(s->size - s->off, s->buf + s->off, stat);
}

int
p9_pack_stat(int bytes, unsigned char *buf, struct p9_stat *stat)
{
  struct p9_stream s = { 0, 0, 0 };

  s.size = bytes;
  s.buf = buf;

  stat->size = 2 + 4 + 13 + 4 + 4 + 4 + 8 + 2 + stat->name_len + 2
      + stat->uid_len + 2 + stat->gid_len + 2 + stat->muid_len;
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
p9_unpack_stat(int bytes, unsigned char *buf, struct p9_stat *stat)
{
  struct p9_stream s = {0, 0, 0};
  int err = 0;

  s.size = bytes;
  s.buf = buf;

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
