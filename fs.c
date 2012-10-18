#include <string.h>
#include <stdlib.h>
#include "9p.h"
#include "util.h"
#include "fs.h"
#include "event.h"
#include "client.h"

unsigned long long qid_cnt = 0;

void
add_file(struct file *root, struct file *f)
{
  f->parent = root;
  if (root) {
    f->next = root->child;
    root->child = f;
  }
}

void
rm_file(struct file *f)
{
  struct file *t, *c;

  if (!f)
    return;

  detach_file_fids(f);
  if (f->owns_name && f->name) {
    free(f->name);
    f->name = 0;
  }
  if (f->parent) {
    if (f->parent->child == f)
      f->parent->child = f->next;
    else {
      for (t = f->parent->child; t && t->next != f; t = t->next) {}
      if (t)
        t->next = f->next;
    }
  }
  for (t = 0, c = f->child; c; c = t) {
    t = c->next;
    c->next = 0;
    rm_file(c);
  }
  f->child = 0;
  if (f->rm)
    f->rm(f);
}

struct file *
get_file(struct file *root, int size, char *name)
{
  struct file *t;

  for (t = root->child; t; t = t->next)
    if (!strncmp(t->name, name, size) && !t->name[size])
      return t;
  return 0;
}

void
free_fid(struct p9_fid *fid)
{
  if (fid->owns_uid && fid->uid)
    free(fid->uid);
  fid->fid = P9_NOFID;
  fid->owns_uid = 0;
  fid->uid = 0;
  if (fid->rm)
    fid->rm(fid);
  fid->file = 0;
}

static void
free_fid_list(struct fid *fids)
{
  struct fid *f, *fnext;
  for (f = fids; f; f = fnext) {
    fnext = f->next;
    free_fid(&f->f);
    free(f);
  }
}

void
free_fids(struct fid_pool *pool)
{
  int i;

  for (i = 0; i < 256; ++i)
    free_fid_list(pool->fids[i]);
  free_fid_list(pool->free);
}

void
reset_fids(struct fid_pool *pool)
{
  struct fid *f, *p = 0;
  int i;

  for (i = 0; i < 256; ++i) {
    for (f = pool->fids[i]; f; f = f->next) {
      free_fid(&f->f);
      p = f;
    }
    if (p) {
      p->next = pool->free;
      pool->free = pool->fids[i];
      pool->fids[i] = 0;
    }
  }
}

struct p9_fid *
get_fid(unsigned int fid, struct fid_pool *pool)
{
  struct fid *f;
  unsigned int i = fid & 0xff;

  for (f = pool->fids[i]; f && f->f.fid != fid; f = f->next) {}
  return f ? &f->f : 0;
}

struct p9_fid *
add_fid(unsigned int fid, struct fid_pool *pool, int msize)
{
  struct fid *f;
  unsigned int i = fid & 0xff;

  if (pool->free) {
    f = pool->free;
    pool->free = pool->free->next;
  } else {
    f = (struct fid *)malloc(sizeof(struct fid));
    if (!f)
      die("Cannot allocate memory");
  }
  memset(f, 0, sizeof(*f));
  f->f.fid = fid;
  f->f.iounit = msize - 23;
  f->f.open_mode = 0;
  f->f.uid = 0;
  f->fprev = f->fnext = 0;
  f->prev = 0;
  f->next = pool->fids[i];
  if (pool->fids[i])
    pool->fids[i]->prev = f;
  pool->fids[i] = f;
  return &f->f;
}

void
rm_fid(struct p9_fid *fid, struct fid_pool *pool)
{
  struct fid *f = (struct fid *)fid;
  unsigned int i;

  if (!fid)
    return;

  i = fid->fid & 0xff;

  if (f == pool->fids[i])
    pool->fids[i] = pool->fids[i]->next;
  else {
    if (f->prev)
      f->prev->next = f->next;
    if (f->next)
      f->next->prev = f->prev;
  }
  detach_fid(fid);
  free_fid(fid);
  f->next = pool->free;
  pool->free = f;
}

void
detach_file_fids(struct file *file)
{
  struct fid *f;

  for (f = file->fids; f; f = f->fnext)
    f->f.file = 0;
}

void
detach_fid(struct p9_fid *fid)
{
  struct file *file;
  struct fid *f = (struct fid *)fid;

  file = (struct file *)fid->file;

  if (!file)
    return;
  if (f == file->fids)
    file->fids = file->fids->fnext;
  if (f->fprev)
    f->fprev->fnext = f->fnext;
  if (f->fnext)
    f->fnext->fprev = f->fprev;
  f->fprev = f->fnext = 0;
  fid->file = 0;
}

void
attach_fid(struct p9_fid *fid, struct file *file)
{
  struct fid *f = (struct fid *)fid;

  if (!fid)
    return;

  fid->file = file;
  f->fprev = 0;
  f->fnext = file->fids;
  if (file->fids)
    file->fids->fprev = f;
  file->fids = f;
}

int
get_req_fid(struct p9_connection *c)
{
  struct client *cl = (struct client *)c;

  c->t.pfid = get_fid(c->t.fid, &cl->fids);
  if (!c->t.pfid) {
    P9_SET_STR(c->r.ename, "fid unknown or out of range");
    return -1;
  }
  if (!c->t.pfid->file) {
    P9_SET_STR(c->r.ename, "file has been removed");
    return -1;
  }
  return 0;
}

static void
fs_version(struct p9_connection *c)
{
  struct client *cl = (struct client *)c;

  if (strncmp(P9_VERSION, c->t.version, c->t.version_len) != 0) {
    c->t.ename = "Protocol not supported";
    c->t.ename_len = strlen(c->t.ename);
    return;
  }
  if (c->msize > c->t.msize)
    c->msize = c->t.msize;
  c->r.msize = c->msize;
  c->r.version = P9_VERSION;
  c->r.version_len = strlen(c->r.version);
  reset_fids(&cl->fids);
}

static void
fs_attach(struct p9_connection *c)
{
  struct client *cl = (struct client *)c;
  struct p9_fid *fid;

  if (strncmp("/", c->t.aname, c->t.aname_len) != 0) {
    c->r.ename = "file not found";
    c->r.ename_len = strlen(c->r.ename);
    return;
  }
  if (get_fid(c->t.fid, &cl->fids)) {
    c->r.ename = "fid already in use";
    c->r.ename_len = strlen(c->r.ename);
    return;
  }
  fid = add_fid(c->t.fid, &cl->fids, c->msize);
  fid->owns_uid = 1;
  fid->uid = strndup(c->t.uname, c->t.uname_len);
  attach_fid(fid, &cl->f);
}

static void
fs_flush(struct p9_connection *c)
{
  struct file *f;

  if (get_req_fid(c))
    return;
  f = (struct file *)c->t.pfid->file;
  if (f->fs && f->fs->flush)
    f->fs->flush(c);
}

static void
fs_walk(struct p9_connection *c)
{
  struct client *cl = (struct client *)c;
  struct p9_fid *newfid = 0;
  struct file *f, *t;
  int i;

  if (get_req_fid(c))
    return;
  if (get_fid(c->t.newfid, &cl->fids) && c->t.newfid != c->t.fid) {
    c->r.ename = "fid already in use";
    c->r.ename_len = strlen(c->r.ename);
    return;
  }

  f = (struct file *)c->t.pfid->file;
  if (c->t.newfid == c->t.fid)
    detach_fid(c->t.pfid);

  if (c->t.nwname == 0) {
    if (c->t.newfid == c->t.fid)
      newfid = c->t.pfid;
    else
      newfid = add_fid(c->t.newfid, &cl->fids, c->msize);
    newfid->uid = c->t.pfid->uid;
    attach_fid(newfid, f);
    return;
  }
  if (!(f->mode & P9_DMDIR)) {
    c->r.ename = "not a directory";
    c->r.ename_len = strlen(c->r.ename);
    return;
  }
  t = f;
  for (i = 0; i < c->t.nwname; ++i) {
    t = get_file(t, c->t.wname_len[i], c->t.wname[i]);
    if (!t)
      break;
    c->r.wqid[c->r.nwqid].type = t->mode >> 24;
    c->r.wqid[c->r.nwqid].version = t->version;
    c->r.wqid[c->r.nwqid].path = t->qpath;
    ++c->r.nwqid;
  }
  if (!c->r.nwqid) {
    c->r.ename = "file not found";
    c->r.ename_len = strlen(c->r.ename);
    return;
  }
  if (c->r.nwqid == c->t.nwname) {
    if (c->t.newfid == c->t.fid)
      newfid = c->t.pfid;
    else
      newfid = add_fid(c->t.newfid, &cl->fids, c->msize);
    newfid->uid = c->t.pfid->uid;
    attach_fid(newfid, t);
  }
}

static void
fs_open(struct p9_connection *c)
{
  struct file *f;

  if (get_req_fid(c))
    return;
  if (c->t.pfid->open_mode) {
    c->r.ename = "file already open for I/O";
    c->r.ename_len = strlen(c->r.ename);
    return;
  }
  f = (struct file *)c->t.pfid->file;
  if ((f->mode & P9_DMDIR) && (((c->t.mode & 3) == P9_OWRITE)
                               || ((c->t.mode & 3) == P9_ORDWR)
                               || (c->t.mode & P9_OTRUNC)
                               || (c->t.mode & P9_ORCLOSE))) {
    c->r.ename = "Is a directory";
    c->r.ename_len = strlen(c->r.ename);
    return;
  }
  /* TODO: check for permissions */
  c->t.pfid->open_mode = c->t.mode;
  c->r.iounit = c->msize - 23;
  c->r.qid.type = f->mode >> 24;
  c->r.qid.version = f->version;
  c->r.qid.path = f->qpath;
  if (f->fs && f->fs->open)
    f->fs->open(c);
}

static void
fs_create(struct p9_connection *c)
{
  struct file *f;

  if (get_req_fid(c))
    return;
  f = (struct file *)c->t.pfid->file;
  if (!(f->mode & P9_DMDIR)) {
    P9_SET_STR(c->r.ename, "Not a directory");
    return;
  }
  if (get_file(f, c->t.name_len, c->t.name)) {
    P9_SET_STR(c->r.ename, "File exists");
    return;
  }
  if (!(f->fs && f->fs->create)) {
    P9_SET_STR(c->r.ename, "Operation not permitted");
    return;
  }
  f->fs->create(c);
}

void
resp_file_create(struct p9_connection *c, struct file *f)
{
  c->t.pfid->open_mode = c->t.mode;
  c->r.iounit = c->msize - 23;
  c->r.qid.type = f->mode >> 24;
  c->r.qid.version = f->version;
  c->r.qid.path = f->qpath;
}

void
file_stat(struct file *f, struct p9_stat *s, char *uid)
{
  memset(s, 0, sizeof(*s));
  s->qid.type = f->mode >> 24;
  s->qid.version = f->version;
  s->qid.path = f->qpath;
  s->mode = f->mode;
  s->atime = 0;
  s->mtime = 0;
  s->length = f->length;
  s->name = f->name;
  s->name_len = strlen(f->name);
  s->uid = uid;
  s->uid_len = strlen(s->uid);
  s->gid = "";
  s->gid_len = strlen(s->gid);
  s->muid = uid;
  s->muid_len = strlen(s->muid);
  s->size = p9_stat_size(s);
}

static void
fs_readdir(struct p9_fid *fid, struct file *dir, struct p9_connection *c)
{
  struct client *cl = (struct client *)c;
  struct file *f;
  unsigned long off, s, count;
  struct p9_stat stat;

  for (off = 0, f = dir->child; f; f = f->next) {
    file_stat(f, &stat, fid->uid);
    s = p9_stat_size(&stat) + 2;
    if (off + s > c->t.offset)
      break;
    off += s;
  }
  if (!f) {
    c->r.count = 0;
    return;
  }
  if (off != c->t.offset) {
    P9_SET_STR(c->r.ename, "Illegal seek");
    return;
  }
  count = c->t.count;
  off = 0;
  for (; f; f = f->next) {
    file_stat(f, &stat, fid->uid);
    if (p9_pack_stat(count, cl->buf + off, &stat))
      break;
    s = stat.size + 2;
    off += s;
    count -= s;
  }
  c->r.data = cl->buf;
  c->r.count = off;
}

static void
fs_read(struct p9_connection *c)
{
  struct file *f;
  int read_mode;

  if (get_req_fid(c))
    return;
  f = (struct file *)c->t.pfid->file;
  if (c->t.count > c->msize - 23)
    c->t.count = c->msize - 23;
  if (f->mode & P9_DMDIR) {
    fs_readdir(c->t.pfid, f, c);
    return;
  }
  read_mode = (((c->t.pfid->open_mode & 3) == P9_OREAD)
               || ((c->t.pfid->open_mode & 3) == P9_ORDWR));
  if (!(f->fs && f->fs->read && read_mode)) {
    c->r.ename = "Operation not permitted";
    c->r.ename_len = strlen(c->r.ename);
    return;
  }
  f->fs->read(c);
}

static void
fs_write(struct p9_connection *c)
{
  struct file *f;
  int write_mode;

  if (get_req_fid(c))
    return;
  f = (struct file *)c->t.pfid->file;
  write_mode = (((c->t.pfid->open_mode & 3) == P9_OWRITE)
                || ((c->t.pfid->open_mode & 3) == P9_ORDWR));
  if ((f->mode & P9_DMDIR) || !(f->fs && f->fs->write) || !write_mode) {
    c->r.ename = "Operation not permitted";
    c->r.ename_len = strlen(c->r.ename);
    return;
  }
  f->fs->write(c);
}

static void
fs_clunk(struct p9_connection *c)
{
  struct client *cl = (struct client *)c;
  struct file *f;

  if (get_req_fid(c) && !c->t.pfid)
    return;
  f = (struct file *)c->t.pfid->file;
  if (f && f->fs && f->fs->clunk)
    f->fs->clunk(c);
  rm_fid(c->t.pfid, &cl->fids);
}

static void
fs_remove(struct p9_connection *c)
{
  struct client *cl = (struct client *)c;
  struct file *f;

  if (get_req_fid(c) && !c->t.pfid)
    return;
  f = (struct file *)c->t.pfid->file;
  if (f) {
    if (f->fs && f->fs->remove)
      f->fs->remove(c);
    rm_file(f);
  }
  rm_fid(c->t.pfid, &cl->fids);
}

static void
fs_stat(struct p9_connection *c)
{
  struct file *f;

  if (get_req_fid(c))
    return;

  f = (struct file *)c->t.pfid->file;
  file_stat(f, &c->r.stat, c->t.pfid->uid);
}

static void
fs_wstat(struct p9_connection *c)
{
  ;
}

struct p9_fs fs = {
  .version = fs_version,
  .auth = 0,
  .attach = fs_attach,
  .flush = fs_flush,
  .walk = fs_walk,
  .open = fs_open,
  .create = fs_create,
  .read = fs_read,
  .write = fs_write,
  .clunk = fs_clunk,
  .remove = fs_remove,
  .stat = fs_stat,
  .wstat = fs_wstat
};

unsigned long long
new_qid(unsigned char type)
{
  return ((++qid_cnt) << 8) | type;
}
