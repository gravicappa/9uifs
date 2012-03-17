#include <string.h>
#include <stdlib.h>
#include "9p.h"
#include "util.h"
#include "fs.h"
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
find_file(struct file *root, int size, char *name)
{
  struct file *t;

  for (t = root->child; t; t = t->next)
    if (!strncmp(t->name, name, size) && !t->name[size])
      return t;
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
  if (cl->msize > c->t.msize)
    cl->msize = c->t.msize;
  c->r.msize = cl->msize;
  c->r.version = P9_VERSION;
  c->r.version_len = strlen(c->r.version);
  reset_fids(cl);
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
  if (get_fid(c->t.fid, cl)) {
    c->r.ename = "fid already in use";
    c->r.ename_len = strlen(c->r.ename);
    return;
  }
  fid = add_fid(c->t.fid, cl);
  fid->owns_uid = 1;
  fid->uid = strndup(c->t.uname, c->t.uname_len);
  fid->context = &cl->fs;
}

static void
fs_flush(struct p9_connection *c)
{
  /* TODO: mark deferred requests as flushed */
}

static void
fs_walk(struct p9_connection *c)
{
  struct client *cl = (struct client *)c;
  struct p9_fid *fid = 0, *newfid = 0;
  struct file *f, *t;
  int i;

  if (get_req_fid(c, &fid))
    return;
  if (get_fid(c->t.newfid, cl)) {
    c->r.ename = "fid already in use";
    c->r.ename_len = strlen(c->r.ename);
    return;
  }
  f = (struct file *)fid->context;
  if (c->t.nwname == 0) {
    newfid = add_fid(c->t.newfid, cl);
    newfid->uid = fid->uid;
    newfid->context = fid->context;
    return;
  }
  if (!(f->mode & P9_DMDIR)) {
    c->r.ename = "not a directory";
    c->r.ename_len = strlen(c->r.ename);
    return;
  }
  log_printf(3, ";fs_walk nwname: %u\n", c->t.nwname);
  t = f;
  for (i = 0; i < c->t.nwname; ++i) {
    t = find_file(t, c->t.wname_len[i], c->t.wname[i]);
    if (!t) {
      log_printf(3, ";   file '%.*s' not found\n", c->t.wname_len[i],
                 c->t.wname[i]);
      break;
    }
    c->r.wqid[c->r.nwqid].type = t->mode >> 24;
    c->r.wqid[c->r.nwqid].version = t->version;
    c->r.wqid[c->r.nwqid].path = t->qpath;
    ++c->r.nwqid;
  }
  log_printf(3, ";   nwqid: %u\n", c->r.nwqid);
  if (!c->r.nwqid) {
    c->r.ename = "file not found";
    c->r.ename_len = strlen(c->r.ename);
    return;
  }
  if (c->r.nwqid == c->t.nwname) {
    newfid = add_fid(c->t.newfid, cl);
    newfid->uid = fid->uid;
    newfid->context = t;
  }
}

static void
fs_open(struct p9_connection *c)
{
  struct p9_fid *fid = 0;
  struct file *f;

  if (get_req_fid(c, &fid))
    return;
  if (fid->open_mode) {
    c->r.ename = "file already open for I/O";
    c->r.ename_len = strlen(c->r.ename);
    return;
  }
  f = (struct file *)fid->context;
  if ((f->mode & P9_DMDIR) && (((c->t.mode & 3) == P9_OWRITE)
                               || ((c->t.mode & 3) == P9_ORDWR)
                               || (c->t.mode & P9_OTRUNC)
                               || (c->t.mode & P9_ORCLOSE))) {
    c->r.ename = "Is a directory";
    c->r.ename_len = strlen(c->r.ename);
    return;
  }
  /* TODO: check for permissions */
  fid->open_mode = c->t.mode;
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
  struct p9_fid *fid = 0;
  struct file *f;

  if (get_req_fid(c, &fid))
    return;
  f = (struct file *)fid->context;
  if (!(f->mode & P9_DMDIR)) {
    c->r.ename = "Not a directory";
    c->r.ename_len = strlen(c->r.ename);
    return;
  }
  if (find_file(f, c->t.name_len, c->t.name)) {
    c->r.ename = "File exists";
    c->r.ename_len = strlen(c->r.ename);
    return;
  }
  if (!(f->fs && f->fs->create)) {
    c->r.ename = "Operation not permitted";
    c->r.ename_len = strlen(c->r.ename);
    return;
  }
  c->t.context = fid;
  f->fs->create(c);
}

void
file_stat(struct file *f, struct p9_stat *s, char *uid)
{
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
fs_readdir(struct p9_fid *fid, struct file *f, struct p9_connection *c)
{
  struct client *cl = (struct client *)c;
  struct file *t;
  unsigned long off = 0, s, count;
  struct p9_stat stat;

  log_printf(3, "fs_readdir off: %u count: %u\n", c->t.offset, c->t.count);

  for (t = f->child; t && off < c->t.offset; t = t->next) {
    log_printf(3, "fs_readdir skipping '%s'\n", t->name);
    file_stat(t, &stat, fid->uid);
    s = p9_stat_size(&stat);
    off += s;
  }
  if (!t) {
    c->r.count = 0;
    return;
  }
  if (off != c->t.offset) {
    P9_SET_STR(c->r.ename, "Illegal seek");
    return;
  }
  off = 0;
  count = c->t.count;
  for (; t; t = t->next) {
    log_printf(3, "; fs_readdir sub: '%s' next: %p\n", t->name, t->next);
    file_stat(t, &stat, fid->uid);
    log_printf(3, ";   stat.size: %u off: %u count: %u\n", stat.size, off,
               count);
    if (p9_pack_stat(count, cl->buf + off, &stat))
      break;
    log_printf(3, "fs_readdir packed\n");
    s = stat.size + 2;
    off += s;
    if (count < s)
      break;
    count -= s;
  }
  c->r.data = cl->buf;
  c->r.count = off;
  log_printf(3, "fs_readdir packed\n");
}

static void
fs_read(struct p9_connection *c)
{
  struct p9_fid *fid = 0;
  struct file *f;

  log_printf(3, "fs_read off: %u count: %u\n", c->t.offset, c->t.count);

  if (get_req_fid(c, &fid))
    return;
  f = (struct file *)fid->context;
  if (f->mode & P9_DMDIR) {
    fs_readdir(fid, f, c);
    return;
  }
  if (!(f->fs && f->fs->read)) {
    c->r.ename = "Operation not permitted";
    c->r.ename_len = strlen(c->r.ename);
    return;
  }
  c->t.context = fid;
  f->fs->read(c);
}

static void
fs_write(struct p9_connection *c)
{
  struct p9_fid *fid = 0;
  struct file *f;

  if (get_req_fid(c, &fid))
    return;
  f = (struct file *)fid->context;
  if ((f->mode & P9_DMDIR) || !(f->fs && f->fs->write)) {
    c->r.ename = "Operation not permitted";
    c->r.ename_len = strlen(c->r.ename);
    return;
  }
  c->t.context = fid;
  f->fs->write(c);
}

static void
fs_clunk(struct p9_connection *c)
{
  struct client *cl = (struct client *)c;
  struct p9_fid *fid = 0;
  struct file *f;

  if (get_req_fid(c, &fid))
    return;
  f = (struct file *)fid->context;
  if (f->fs && f->fs->clunk) {
    c->t.context = fid;
    f->fs->clunk(c);
  }
  rm_fid(c->t.fid, cl);
}

static void
fs_remove(struct p9_connection *c)
{
  struct client *cl = (struct client *)c;
  struct p9_fid *fid = 0;
  struct file *f;

  if (get_req_fid(c, &fid))
    return;
  f = (struct file *)fid->context;
  if (f->fs && f->fs->remove) {
    c->t.context = fid;
    f->fs->remove(c);
  }
  rm_file(f);
  rm_fid(c->t.fid, cl);
}

static void
fs_stat(struct p9_connection *c)
{
  struct p9_fid *fid = 0;
  struct file *f;

  if (get_req_fid(c, &fid))
    return;

  f = (struct file *)fid->context;

  file_stat(f, &c->r.stat, fid->uid);
  log_printf(3, "; fs_stat: size: %d\n", c->r.stat.size);
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
