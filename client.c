#include <string.h>
#include <stdlib.h>

#ifdef WIN32
#include <winsock2.h>
#define socklen_t int
#else
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#endif

#include "util.h"
#include "9p.h"
#include "9pio.h"
#include "fs.h"
#include "client.h"
#include "net.h"

struct fs_entry *fs_client[] = {
  {".", P9_QTDIR, 0, 0500, 0},
  {"ctl", 0, 0, 0600, 0},
  {"event", 0, 0, 0600, 0},
  {"views", P9_QTDIR, 0, 0500, 0},
  {"images", P9_QTDIR, 0, 0500, 0},
  {0}
};

struct buf clients = {16 * sizeof(struct client)};

struct client *
add_client(int server_fd, int msize)
{
  struct client *c;
  int off, fd;
  struct sockaddr_in addr;
  socklen_t addr_len;

  addr_len = sizeof(addr);
  fd = accept(server_fd, (struct sockaddr *)&addr, &addr_len);
  if (fd < 0)
    return 0;
  log_printf(3, "; Incoming connection\n");
  off = add_data(&clients, sizeof(struct client), 0);
  if (off < 0)
    die("Cannot allocate memory\n");
  c = (struct client *)(clients.b + off);
  memset(c, 0, sizeof(*c));
  c->fd = fd;
  c->fids.delta = 16 * sizeof(unsigned long);
  c->read = 0;
  c->size = 0;
  c->msize = msize;
  c->inbuf = (char *)malloc(msize);
  c->outbuf = (char *)malloc(msize);

  c->fs.mode = P9_DMDIR | 0500;
  c->fs.qpath = ++qid_cnt;
  c->fs.context = c;

  c->fs_event.name = "event";
  c->fs_event.mode = 0400;
  c->fs_event.qpath = ++qid_cnt;
  c->fs_event.context = c;
  add_file(&c->fs_event, &c->fs);

  c->fs_views.name = "views";
  c->fs_views.mode = P9_DMDIR | 0700;
  c->fs_views.qpath = ++qid_cnt;
  c->fs_views.context = c;
  add_file(&c->fs_views, &c->fs);

  c->fs_images.name = "images";
  c->fs_images.mode = P9_DMDIR | 0700;
  c->fs_images.qpath = ++qid_cnt;
  c->fs_images.context = c;
  add_file(&c->fs_images, &c->fs);

  c->fs_fonts.name = "fonts";
  c->fs_fonts.mode = P9_DMDIR | 0700;
  c->fs_fonts.qpath = ++qid_cnt;
  c->fs_fonts.context = c;
  add_file(&c->fs_fonts, &c->fs);

  c->fs_comm.name = "comm";
  c->fs_comm.mode = P9_DMDIR | 0700;
  c->fs_comm.qpath = ++qid_cnt;
  c->fs_comm.context = c;
  add_file(&c->fs_comm, &c->fs);

  return c;
}

void
rm_client(struct client *c)
{
  int off, end;

  log_printf(3, "; rm_client %p\n", c);

  if (!c)
    return;
  if (c->fd >= 0)
    close(c->fd);
  if (c->inbuf)
    free(c->inbuf);
  if (c->outbuf)
    free(c->outbuf);
  if (c->fids.b)
    free(c->fids.b);
  rm_buf(&clients, sizeof(struct client), c);
}

unsigned int
unpack_uint4(unsigned char *buf)
{
  return buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
}

int
process_client(struct client *c)
{
  int r;
  unsigned int size, outsize, msize = c->msize;
  char *inbuf = c->inbuf, *outbuf = c->outbuf;

  r = recv(c->fd, inbuf + c->read, msize - c->read, 0);
  if (r <= 0)
    return -1;
  c->read += r;
  if (c->read < 4)
    return 0;
  do {
    log_printf(3, "; <- ");
    log_print_data(3, c->read, (unsigned char *)inbuf);
    size = unpack_uint4((unsigned char *)inbuf);
    log_printf(3, ";   size: %d\n", size);
    if (size < 7 || size > msize)
      return -1;
    log_printf(3, ";   c->read: %d\n", c->read);
    if (size > c->read)
      return 0;
    if (p9_process_srv(msize, c->inbuf, msize, c->outbuf, &c->c, &fs))
      return -1;
    outsize = unpack_uint4((unsigned char *)c->outbuf);
    log_printf(3, "; -> ");
    log_print_data(3, outsize, (unsigned char *)outbuf);
    if (send(c->fd, outbuf, outsize, 0) <= 0)
      return -1;
    memmove(inbuf, inbuf + size, msize - size);
    c->read -= size;
  } while (c->read);
}

void
free_fid(struct p9_fid *fid)
{
  if (fid->owns_uid && fid->uid)
    free(fid->uid);
}

void
reset_fids(struct client *c)
{
  struct p9_fid *fids = (struct p9_fid *)c->fids.b;
  int i, n = c->fids.used / sizeof(struct p9_fid);

  for (i = 0; i < n; ++i)
    free_fid(fids[i]);
  c->fids.used = 0;
}

struct p9_fid *
get_fid(unsigned int fid, struct client *c)
{
  struct p9_fid *fids = (struct p9_fid *)c->fids.b;
  int i, n = c->fids.used / sizeof(struct p9_fid);

  for (i = 0; i < n; ++i)
    if (fids[i].fid == fid)
      return &fids[i];
  return 0;
}

int
get_req_fid(struct p9_connection *c, struct p9_fid **fid)
{
  struct client *cl = (struct client *)c;

  *fid = get_fid(c->t.fid, cl);
  if (!*fid) {
    P9_SET_STR(c->r.ename, "fid unknown or out of range");
    return -1;
  }
  return 0;
}

struct p9_fid *
add_fid(unsigned int fid, struct client *c)
{
  int off;
  struct p9_fid *f;

  off = add_data(&c->fids, sizeof(struct p9_fid), 0);
  if (off < 0)
    die("Cannot allocate memory\n");
  f = (struct p9_fid *)(c->fids.b + off);
  f->fid = fid;
  f->context = c;
  f->iounit = c->msize;
  f->open_mode = 0;
  f->uid = 0;
  return f;
}

void
rm_fid(struct p9_fid *fid, struct client *c)
{
  free_fid(fid);
  rm_data(&c->fids, sizeof(struct p9_fid), fid);
}
