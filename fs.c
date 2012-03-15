#include <string.h>
#include "9p.h"
#include "util.h"
#include "client.h"
#include "fs.h"

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
  for (t = root->child; t && strncmp(t->name, name, size); t = t->next) {}
  return t;
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
  if (!f->mode & P9_DMDIR) {
    c->r.ename = "not a directory";
    c->r.ename_len = strlen(c->r.ename);
    return;
  }
  t = f;
  for (i = 0; i < c->t.nwname; ++i) {
    t = find_file(t, c->t.nwname_len[i], c->t.nwname[i]);
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
  if (c->r.nwqid == c->t.nwqid) {
    newfid = add_fid(c->t.newfid, cl);
    newfid->uid = fid->uid;
    newfid->context = t;
  }
}

static void
fs_open(struct p9_connection *c)
{
  struct client *cl = (struct client *)c;
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
    c->r.ename = strlen(c->r.ename);
    return;
  }
  /* TODO: check for permissions */
  fid->open_mode = c->t.mode;
  c->r.iounit = c->msize - 23;
  c->r.qid.type = f->mode >> 24;
  c->r.qid.version = f->version;
  c->r.qid.path = f->qpath;
}

static void
fs_create(struct p9_connection *c)
{
  struct client *cl = (struct client *)c;
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

static void
fs_read(struct p9_connection *c)
{
  struct client *cl = (struct client *)c;
  struct p9_fid *fid = 0;
  struct file *f;

  if (get_req_fid(c, &fid))
    return;
  f = (struct file *)fid->context;
  if (f->mode & P9_DMDIR) {
    fs_readdir(f, c);
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
  struct client *cl = (struct client *)c;
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
  rm_fid(fid, cl);
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
  rm_fid(fid, cl);
}

static void
fs_stat(struct p9_connection *c)
{
  struct client *cl = (struct client *)c;
  struct p9_fid *fid = 0;
  struct file *f;

  if (get_req_fid(c, &fid))
    return;

  c->r.stat.qid.type = f->mode >> 24;
  c->r.stat.qid.version = f->version;
  c->r.stat.qid.path = f->qpath;
  c->r.stat.mode = f->mode;
  c->r.stat.atime = 0;
  c->r.stat.mtime = 0;
  c->r.stat.length = f->length;
  c->r.stat.name = f->name;
  c->r.stat.name_len = strlen(f->name);
  c->r.stat.uid = fid->uid;
  c->r.stat.uid_len = strlen(c->r.stat.uid);
  c->r.stat.gid = "";
  c->r.stat.gid_len = strlen(c->r.stat.gid);
  c->r.stat.muid = fid->uid;
  c->r.stat.muid_len = strlen(c->r.stat.muid);
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
