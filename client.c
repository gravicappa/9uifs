#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "util.h"
#include "net.h"
#include "api.h"
#include "frontend.h"
#include "9p.h"
#include "9pdbg.h"
#include "fs.h"
#include "fstypes.h"
#include "fsutil.h"
#include "client.h"
#include "ctl.h"
#include "ui.h"
#include "config.h"
#include "bus.h"
#include "font.h"
#include "images.h"
#include "profile.h"

struct client *wm_client = 0;
struct client *clients = 0;
unsigned int cur_time_ms;

static void rm_client(struct client *c);
static void sysctl_set_desktop(char *cmd, void *aux);
static void sysctl_set_wm(char *cmd, void *aux);
static void sysctl_msg(char *cmd, void *aux);
static void sysctl_msg_bcast(char *cmd, void *aux);
static void sysctl_grab_key(char *cmd, void *aux);
static void sysctl_ungrab_key(char *cmd, void *aux);

static struct ctl_cmd sys_ctl[] = {
  {"msg", sysctl_msg},
  {"bcast", sysctl_msg_bcast},
  {"set_desktop", sysctl_set_desktop},
  {"set_wm", sysctl_set_wm},
  {"grab_key", sysctl_grab_key},
  {"ungrab_key", sysctl_ungrab_key},
  {0}
};

struct client *
add_client(int fd, int msize)
{
  struct client *c;
  c = calloc(1, sizeof(struct client));
  if (!c)
    die("Cannot allocate memory");
  c->fd = fd;
  c->con.msize = msize;
  c->inbuf = malloc(msize);
  c->outbuf = malloc(msize);
  c->con.buf = malloc(msize);

  if (!(c->inbuf && c->outbuf && c->con.buf))
    die("Cannot allocate memory");

  memset(c->con.buf, 0xff, msize);

  c->f.name = "/";
  c->f.mode = 0500 | P9_DMDIR;
  c->f.qpath = new_qid(FS_ROOT);

  c->bus = mk_bus("bus", c);
  if (!c->bus)
    goto error;
  add_file(&c->f, c->bus);
  add_file(c->bus, mk_ctl("sys", sys_ctl, c));

  c->images = mk_image_dir("images");
  if (!c->images)
    goto error;
  add_file(&c->f, c->images);

  c->fonts = mk_fonts_fs("fonts");
  if (!c->fonts)
    goto error;
  add_file(&c->f, c->fonts);

  c->ui = mk_ui("ui");
  if (!c->ui)
    goto error;
  add_file(&c->f, c->ui);

  if (clients)
    clients->prev = c;
  c->next = clients;
  clients = c;

  log_printf(LOG_CLIENT, "# Added new client (fd: %d)\n", fd);
  return c;
error:
  rm_client(c);
  return 0;
}

static struct client *
find_other_wm(struct client *c)
{
  struct client *t;
  for (t = clients; t; t = t->next)
    if (t != c && t->flags & CLIENT_WM)
      return t;
  return 0;
}

static void
rm_client(struct client *c)
{
  if (!c)
    return;

  log_printf(LOG_CLIENT, "# Removing client %p (fd: %d)\n", c, c->fd);
  if ((c->flags & CLIENT_WM) && wm_client == c) {
    wm_client = find_other_wm(c);
    wm_ungrab_keys();
  }
  free_fids(&c->fids);
  rm_file(&c->f);
  if (c->fd >= 0)
    close(c->fd);
  if (c->inbuf)
    free(c->inbuf);
  if (c->outbuf)
    free(c->outbuf);
  if (c->con.buf)
    free(c->con.buf);
  if (!clients) {
    free(c);
    return;
  }
  if (c->prev)
    c->prev->next = c->next;
  else
    clients = c->next;
  if (c->next)
    c->next->prev = c->prev;
  free(c);
}

static unsigned int
unpack_uint4(unsigned char *buf)
{
  return buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
}

int
client_send_resp(struct client *c)
{
  unsigned int outsize;

  profile_start(PROF_IO_PACK);
  if (p9_pack_msg(c->con.msize, (char *)c->outbuf, &c->con.r))
    return -1;
#if 0
  p9_print_msg(&c->con.r, "OUT");
#endif
  profile_end(PROF_IO_PACK);
  outsize = unpack_uint4((unsigned char *)c->outbuf);
  profile_end2(PROF_IO_CLIENT);
  profile_start(PROF_IO_SEND);
  /* TODO: handle EAGAIN and EWOULDBLOCK */
  if (send(c->fd, c->outbuf, outsize, 0) <= 0) {
    profile_end(PROF_IO_SEND);
    profile_start(PROF_IO_CLIENT);
    return -1;
  }
  profile_end(PROF_IO_SEND);
  profile_start(PROF_IO_CLIENT);
  return 0;
}

static int
process_client_io(struct client *c)
{
  int r;
  unsigned int size, off;
  unsigned char *buf = c->inbuf;

  profile_start(PROF_IO_CLIENT);
  if (c->read >= c->con.msize - (c->con.msize >> 4)) {
    profile_start(PROF_IO_MEMMOVE);
    if (c->read > c->off)
      memmove(buf, buf + c->off, c->read - c->off);
    c->read -= c->off;
    c->off = 0;
    profile_end(PROF_IO_MEMMOVE);
  }
  profile_start(PROF_IO_READ);
  r = recv(c->fd, buf + c->read, c->con.msize - c->read, 0);
  profile_end(PROF_IO_READ);
  if (r == 0) {
    profile_end(PROF_IO_CLIENT);
    return 1;
  }
  if (r <= 0) {
    profile_end(PROF_IO_CLIENT);
    return (net_wouldblock()) ? 0 : -1;
  }
  c->read += r;
  if (c->read < 7) {
    profile_end(PROF_IO_CLIENT);
    return 0;
  }
  off = c->off;
  while (c->read - off >= 7) {
    size = unpack_uint4((unsigned char *)buf + off);
    if (size < 7 || size > c->con.msize) {
      log_printf(LOG_DBG, "wrong message size size: %u msize: %u\n",
                 size, c->con.msize);
      profile_end(PROF_IO_CLIENT);
      return -1;
    }
    if (off + size > c->read)
      break;
    profile_start(PROF_IO_UNPACK);
    if (p9_unpack_msg(size, (char *)buf + off, &c->con.t)) {
      profile_end(PROF_IO_UNPACK);
      profile_end(PROF_IO_CLIENT);
      log_printf(LOG_DBG, "unpack message error\n");
      return -1;
    }
#if 0
    p9_print_msg(&c->con.t, "IN");
#endif
    profile_end(PROF_IO_UNPACK);
    c->con.r.deferred = 0;
    profile_start(PROF_IO_PROCESS);
    if (p9_process_treq(&c->con, &fs)) {
      profile_end(PROF_IO_PROCESS);
      profile_end(PROF_IO_CLIENT);
      log_printf(LOG_DBG, "process message error\n");
      return -1;
    }
    profile_end(PROF_IO_PROCESS);
    if (!c->con.r.deferred && client_send_resp(c))
      return -1;
    off += size;
  }
  c->off = off;
  profile_end(PROF_IO_CLIENT);
  return 0;
}

static int
update_sock_set(fd_set *fdset, int server_fd, int event_fd)
{
  struct client *c;
  int m;

  FD_ZERO(fdset);
  FD_SET(server_fd, fdset);
  FD_SET(event_fd, fdset);
  m = (server_fd > event_fd) ? server_fd : event_fd;
  for (c = clients; c; c = c->next) {
    FD_SET(c->fd, fdset);
    m = (m > c->fd) ? m : c->fd;
  }
  return m;
}

static struct client *
accept_client(int server_fd)
{
  int fd, x = 1;
  struct sockaddr_in addr;
  socklen_t len = sizeof(addr);
  struct client *c;

  fd = accept(server_fd, (struct sockaddr *)&addr, &len);
  if (fd < 0 || nonblock_socket(fd))
    return 0;
  setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &x, sizeof(x));
  log_printf(LOG_CLIENT, "# Incoming connection (fd: %d)\n", fd);
  c = add_client(fd, MSIZE);
  if (c && addr.sin_addr.s_addr == htonl(INADDR_LOOPBACK))
    c->flags |= CLIENT_LOCAL;
  return c;
}

int
uifs_process_io(int srvfd, int evfd, unsigned int frame_ms)
{
  struct timeval tv;
  fd_set fdset;
  int m, r;
  struct client *c, *cnext;
  unsigned char evbuf[1];

  profile_start(PROF_IO_DEFERRED);
  for (c = clients; c; c = c->next)
    send_events_deferred(c->bus);
  profile_end(PROF_IO_DEFERRED);
  profile_start(PROF_IO_UPD_FDSET);
  m = update_sock_set(&fdset, srvfd, evfd);
  profile_end(PROF_IO_UPD_FDSET);
  if (ui_update_list) {
    tv.tv_sec = 0;
    tv.tv_usec = 1000 * frame_ms;
  }
  profile_end2(PROF_IO);
  r = select(m + 1, &fdset, 0, 0, (ui_update_list) ? &tv : 0);
  profile_start(PROF_IO);
  if (r < 0 && errno != EINTR && errno != 514)
    return -1;
  cur_time_ms = current_time_ms();
  if (r > 0) {
    if (FD_ISSET(srvfd, &fdset))
      accept_client(srvfd);
    if (FD_ISSET(evfd, &fdset))
      read(evfd, evbuf, sizeof(evbuf));
    for (c = clients; c; c = cnext) {
      cnext = c->next;
      if (FD_ISSET(c->fd, &fdset))
        if (process_client_io(c))
          rm_client(c);
    }
  }
  return 0;
}

struct client *
client_by_id(unsigned long long id)
{
  struct client *c;
  for (c = clients; c && c->f.qpath != id; c = c->next) {}
  return c;
}

void
sysctl_set_wm(char *cmd, void *aux)
{
  struct client *c = aux;
  log_printf(LOG_DBG, "set_wm '%s' \n", cmd);
  if (!(c->flags & CLIENT_LOCAL))
    return;
  if (!strcmp(cmd, "1")) {
    c->flags |= CLIENT_WM;
    wm_client = (wm_client) ? wm_client : c;
  } else if (c->flags & CLIENT_WM) {
    c->flags &= ~CLIENT_WM;
    if (wm_client == c)
      wm_client = find_other_wm(c);
  }
}

void
sysctl_set_desktop(char *cmd, void *aux)
{
  struct uiobj *u;
  struct client *c = aux;

  if (!(c->flags & CLIENT_WM))
    return;
  u = find_uiobj(cmd, c);
  log_printf(LOG_DBG, "set_desktop '%s' /  %p\n", cmd, u);
  if (!u)
    return;
  ui_set_desktop(u);
}

void
sysctl_msg(char *cmd, void *aux)
{
  struct client *to;
  unsigned long long id;
  char *sid;

  sid = next_arg(&cmd);
  if (!(sid && cmd && sscanf(sid, "%llu", &id) == 1))
    return;
  to = client_by_id(id);
  if (to)
    put_event_str(to->bus, bus_ch_ev, strlen(cmd), cmd);
}

void
sysctl_msg_bcast(char *cmd, void *aux)
{
  struct client *to;
  log_printf(LOG_DBG, "bcast '%s'\n", cmd);
  for (to = clients; to; to = to->next)
    if (to != aux)
      put_event_str(to->bus, bus_ch_ev, strlen(cmd), cmd);
}

void
sysctl_grab_key(char *cmd, void *aux)
{
  unsigned int key, mod;
  if ((struct client *)aux != wm_client
      || sscanf(cmd, "%u %u", &key, &mod) != 2)
    return;
  wm_grab_key(key, mod);
}

void
sysctl_ungrab_key(char *cmd, void *aux)
{
  unsigned int key, mod;
  if ((struct client *)aux != wm_client
      || sscanf(cmd, "%u %u", &key, &mod) != 2)
    return;
  wm_ungrab_key(key, mod);
}
