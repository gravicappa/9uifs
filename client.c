#include <string.h>
#include <stdlib.h>
#include <unistd.h>

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
#include "9pdbg.h"
#include "fs.h"
#include "client.h"
#include "net.h"
#include "surface.h"
#include "event.h"
#include "view.h"

struct client *clients = 0;
struct client *selected_client = 0;

static void free_fid(struct p9_fid *fid);

struct client *
add_client(int server_fd, int msize)
{
  struct client *c;
  int fd;
  struct sockaddr_in addr;
  socklen_t addr_len;

  addr_len = sizeof(addr);
  fd = accept(server_fd, (struct sockaddr *)&addr, &addr_len);
  if (fd < 0)
    return 0;
  log_printf(3, "# Incoming connection (fd: %d)\n", fd);
  c = (struct client *)malloc(sizeof(struct client));
  if (!c)
    die("Cannot allocate memory");
  memset(c, 0, sizeof(*c));
  c->fd = fd;
  c->read = 0;
  c->size = 0;
  c->c.msize = msize;
  c->inbuf = (char *)malloc(msize);
  c->outbuf = (char *)malloc(msize);
  c->buf = (char *)malloc(msize);

  if (!(c->inbuf && c->outbuf && c->buf))
    die("Cannot allocate memory");

  c->fs.name = "/";
  c->fs.mode = 0500 | P9_DMDIR;
  c->fs.qpath = ++qid_cnt;
  c->fs.aux.p = c;

  c->fs_event.name = "event";
  c->fs_event.mode = 0400;
  c->fs_event.qpath = ++qid_cnt;
  c->fs_event.aux.p = c;
  add_file(&c->fs, &c->fs_event);

  c->fs_views.name = "views";
  c->fs_views.mode = 0700 | P9_DMDIR;
  c->fs_views.qpath = ++qid_cnt;
  c->fs_views.fs = &fs_views;
  c->fs_views.aux.p = c;
  add_file(&c->fs, &c->fs_views);

  c->fs_images.name = "images";
  c->fs_images.mode = 0700 | P9_DMDIR;
  c->fs_images.qpath = ++qid_cnt;
  c->fs_images.aux.p = c;
  add_file(&c->fs, &c->fs_images);

  c->fs_fonts.name = "fonts";
  c->fs_fonts.mode = 0700 | P9_DMDIR;
  c->fs_fonts.qpath = ++qid_cnt;
  c->fs_fonts.aux.p = c;
  add_file(&c->fs, &c->fs_fonts);

  c->fs_comm.name = "comm";
  c->fs_comm.mode = 0700 | P9_DMDIR;
  c->fs_comm.qpath = ++qid_cnt;
  c->fs_comm.aux.p = c;
  add_file(&c->fs, &c->fs_comm);

  c->next = clients;
  clients = c;

  log_printf(3, "# Added new client (fd: %d)\n", fd);
  selected_client = c;
  return c;
}

static void
free_fids(struct p9_fid *fids)
{
  struct p9_fid *f, *fnext;
  for (f = fids; f; f = fnext) {
    fnext = f->next;
    free_fid(f);
    free(f);
  }
}

void
rm_client(struct client *c)
{
  struct client *p;
  log_printf(3, "; rm_client %p\n", c);

  if (!c)
    return;

  if (c->fd >= 0)
    close(c->fd);
  if (c->inbuf)
    free(c->inbuf);
  if (c->outbuf)
    free(c->outbuf);
  if (c->buf)
    free(c->buf);
  free_fids(c->fids);
  free_fids(c->fids_pool);
  if (!clients) {
    free(c);
    return;
  }

  for (p = clients; p != c && p->next; p = p->next) {}
  if (p == clients)
    clients = clients->next;
  else
    p->next = c->next;
  free(c);
}

unsigned int
unpack_uint4(unsigned char *buf)
{
  return buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
}

int
process_client_io(struct client *c)
{
  int r;
  unsigned int size;

  r = recv(c->fd, c->inbuf + c->read, c->c.msize - c->read, 0);
  if (r <= 0)
    return -1;
  c->read += r;
  if (c->read < 4)
    return 0;
  do {
    log_printf(10, "; <- ");
    log_print_data(10, c->read, (unsigned char *)c->inbuf);
    size = unpack_uint4((unsigned char *)c->inbuf);
    log_printf(10, ";   size: %d\n", size);
    if (size < 7 || size > c->c.msize)
      return -1;
    log_printf(10, ";   c->read: %d\n", c->read);
    if (size > c->read)
      return 0;

    if (p9_unpack_msg(c->c.msize, c->inbuf, &c->c.t))
      return -1;
    if (loglevel >= 9)
      p9_print_msg(&c->c.t, ">>");

    c->c.r.deferred = 0;
    if(p9_process_treq(&c->c, &fs))
      return -1;
    if (!c->c.r.deferred && client_send_resp(c))
      return -1;

    memmove(c->inbuf, c->inbuf + size, c->c.msize - size);
    c->read -= size;
  } while (c->read);
  return 0;
}

int
client_send_resp(struct client *c)
{
  unsigned int outsize;

  if (p9_pack_msg(c->c.msize, c->outbuf, &c->c.r))
    return -1;
  if (loglevel >= 9)
    p9_print_msg(&c->c.r, ">>");
  outsize = unpack_uint4((unsigned char *)c->outbuf);
  log_printf(10, "; -> ");
  log_print_data(10, outsize, (unsigned char *)c->outbuf);
  if (send(c->fd, c->outbuf, outsize, 0) <= 0)
    return -1;
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

void
reset_fids(struct client *c)
{
  struct p9_fid *f = c->fids, *p = 0;

  for (f = c->fids; f; f = f->next) {
    free_fid(f);
    p = f;
  }
  if (p) {
    p->next = c->fids_pool;
    c->fids_pool = c->fids;
    c->fids = 0;
  }
}

struct p9_fid *
get_fid(unsigned int fid, struct client *c)
{
  struct p9_fid *f;

  for (f = c->fids; f && f->fid != fid; f = f->next) {}
  return f;
}

struct p9_fid *
add_fid(unsigned int fid, struct client *c)
{
  struct p9_fid *f;

  if (c->fids_pool) {
    f = c->fids_pool;
    c->fids_pool = c->fids_pool->next;
  } else {
    f = (struct p9_fid *)malloc(sizeof(struct p9_fid));
    if (!f)
      die("Cannot allocate memory");
  }
  memset(f, 0, sizeof(*f));
  f->fid = fid;
  f->iounit = c->c.msize - 23;
  f->open_mode = 0;
  f->uid = 0;
  f->next = c->fids;
  c->fids = f;
  return f;
}

void
rm_fid(struct p9_fid *fid, struct client *c)
{
  struct p9_fid *prev, *f;

  prev = 0;
  for (f = c->fids; f && f != fid; f = f->next)
    prev = f;
  if (f) {
    if (prev)
      prev->next = f->next;
    else
      c->fids = f->next;
  }
  free_fid(fid);
  fid->next = c->fids_pool;
  c->fids_pool = fid;
}

void
client_keyboard(int type, int keysym, int mod, unsigned int unicode)
{
  struct view *v;
  char buf[64];
  int len;

  if (!(selected_client && selected_client->selected_view))
    return;
  v = selected_client->selected_view;
  len = snprintf(buf, sizeof(buf), "%c %u %u %u\n", type, keysym, mod,
                 unicode);
  put_event(selected_client, &v->fs_keyboard, len, buf);
}

void
client_pointer_move(int x, int y, int state)
{
  struct view *v;
  char buf[48];
  int len;

  if (!(selected_client && selected_client->selected_view))
    return;
  v = selected_client->selected_view;
  len = snprintf(buf, sizeof(buf), "m %u %u %u\n", x, y, state);
  put_event(selected_client, &v->fs_pointer, len, buf);
}

void
client_pointer_click(int type, int x, int y, int btn)
{
  struct view *v;
  char buf[48];
  int len;

  if (!(selected_client && selected_client->selected_view))
    return;
  v = selected_client->selected_view;
  len = snprintf(buf, sizeof(buf), "%c %u %u %u\n", type, x, y, btn);
  put_event(selected_client, &v->fs_pointer, len, buf);
}
