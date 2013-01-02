#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <Imlib2.h>
#include <errno.h>

#include "util.h"
#include "net.h"
#include "input.h"
#include "9p.h"
#include "9pdbg.h"
#include "fs.h"
#include "fstypes.h"
#include "event.h"
#include "client.h"
#include "ctl.h"
#include "draw.h"
#include "surface.h"
#include "prop.h"
#include "ui.h"
#include "view.h"
#include "config.h"
#include "font.h"
#include "images.h"
#include "wm.h"

struct client *clients = 0;
unsigned int cur_time_ms;

struct client *
add_client(int server_fd, int msize)
{
  struct client *c;
  int fd, opt = 1;
  struct sockaddr_in addr;
  socklen_t len;

  len = sizeof(addr);
  fd = accept(server_fd, (struct sockaddr *)&addr, &len);
  if (fd < 0 || nonblock_socket(fd))
    return 0;
  setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
  log_printf(LOG_CLIENT, "# Incoming connection (fd: %d)\n", fd);
  c = (struct client *)calloc(1, sizeof(struct client));
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

  c->ev.f.name = "event";
  init_event(&c->ev, c);
  add_file(&c->f, &c->ev.f);

  c->f_views.name = "views";
  c->f_views.mode = 0700 | P9_DMDIR;
  c->f_views.qpath = new_qid(FS_VIEWS);
  c->f_views.fs = &fs_views;
  add_file(&c->f, &c->f_views);

  c->images = init_image_dir("images");
  add_file(&c->f, c->images);

  if (init_fonts_fs(&c->f_fonts) == 0) {
    c->f_fonts.name = "fonts";
    add_file(&c->f, &c->f_fonts);
  }

  c->f_comm.name = "comm";
  c->f_comm.mode = 0700 | P9_DMDIR;
  c->f_comm.qpath = new_qid(FS_NONE);
  add_file(&c->f, &c->f_comm);

  if (ui_init_ui(c) == 0) {
    c->ui->name = "ui";
    add_file(&c->f, c->ui);
  }
  if (clients)
    clients->prev = c;
  c->next = clients;
  clients = c;

  log_printf(LOG_CLIENT, "# Added new client (fd: %d)\n", fd);
  return c;
}

void
rm_client(struct client *c)
{
  if (!c)
    return;

  log_printf(LOG_CLIENT, "# Removing client %p (fd: %d)\n", c, c->fd);
  if (c->fd >= 0)
    close(c->fd);
  if (c->inbuf)
    free(c->inbuf);
  if (c->outbuf)
    free(c->outbuf);
  if (c->con.buf)
    free(c->con.buf);
  free_fids(&c->fids);
  rm_file(&c->f);
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

  if (p9_pack_msg(c->con.msize, c->outbuf, &c->con.r))
    return -1;
  outsize = unpack_uint4((unsigned char *)c->outbuf);
  if (send(c->fd, c->outbuf, outsize, 0) <= 0)
    return -1;
  return 0;
}

static int
process_client_io(struct client *c)
{
  int r;
  unsigned int size, off;
  char *buf = c->inbuf;

  if (c->read >= c->con.msize - (c->con.msize >> 4)) {
    if (c->read > c->off)
      memmove(buf, buf + c->off, c->read - c->off);
    c->read -= c->off;
    c->off = 0;
  }
  r = recv(c->fd, buf + c->read, c->con.msize - c->read, 0);
  if (r == 0)
    return 1;
  if (r <= 0)
    return (net_wouldblock())? 0 : -1;
  c->read += r;
  if (c->read < 4)
    return 0;
  off = c->off;
  while (c->read - off >= 4) {
    size = unpack_uint4((unsigned char *)buf + off);
    if (size < 7 || size > c->con.msize) {
      log_printf(LOG_DBG, "wrong message size size: %u msize: %u\n",
                 size, c->con.msize);
      return -1;
    }
    if (off + size > c->read)
      break;
    if (p9_unpack_msg(size, buf + off, &c->con.t)) {
      log_printf(LOG_DBG, "unpack message error\n");
      return -1;
    }
    c->con.r.deferred = 0;
    if (p9_process_treq(&c->con, &fs)) {
      log_printf(LOG_DBG, "process message error\n");
      return -1;
    }
    if (!c->con.r.deferred && client_send_resp(c))
      return -1;
    off += size;
  }
  c->off = off;
  return 0;
}

void
client_input_event(struct input_event *ev)
{
  wm_on_input(ev);
}

static int
update_views(struct client *c)
{
  struct file *vf;
  struct view *v;
  int changed = 0;

  for (vf = c->f_views.child; vf; vf = vf->next) {
    v = (struct view *)vf;
    if ((v->flags & VIEW_VISIBLE) && (v->flags & VIEW_DIRTY)) {
      v->flags |= VIEW_EV_DIRTY;
      ui_update_view(v);
      changed = 1;
    }
  }
  return changed;
}

static int
draw_views(struct client *c)
{
  struct file *vf;
  struct view *v;
  int changed = 0;

  for (vf = c->f_views.child; vf; vf = vf->next) {
    v = (struct view *)vf;
    if (v->flags & VIEW_VISIBLE)
      changed |= draw_view((struct view *)vf);
    v->flags &= ~VIEW_DIRTY;
  }
  return changed;
}

int
draw_clients()
{
  struct client *c;
  int changed = 0;

  clean_dirty_rects();
  for (c = clients; c; c = c->next) {
    update_views(c);
    changed |= draw_views(c);
  }
  changed |= ui_update();
  return changed;
}

void
blit_clients(int rect[4])
{
  struct client *c;
  struct view *v;
  struct file *f;
  struct screen *s = default_screen();
  int r[4], x, y;

  for (c = clients; c; c = c->next) {
    for (f = c->f_views.child; f; f = f->next) {
      v = (struct view *)f;
      ui_intersect_clip(r, v->g.r, rect);
      x = v->g.r[0];
      y = v->g.r[1];
      blit_image(s->blit, r[0], r[1], r[2], r[3],
                 v->blit.img, r[0] - x, r[1] - y, r[2], r[3]);
    }
  }
}

int
update_sock_set(fd_set *fdset, int server_fd)
{
  struct client *c = clients;
  int m;

  FD_ZERO(fdset);
  FD_SET(server_fd, fdset);

  m = server_fd;
  for (c = clients; c; c = c->next) {
    FD_SET(c->fd, fdset);
    m = (m > c->fd) ? m : c->fd;
  }
  return m;
}

int
process_clients(int server_fd, unsigned int time_ms, unsigned int frame_ms)
{
  struct timeval tv;
  fd_set fdset;
  int m, r;
  struct client *c, *cnext;

  for (c = clients; c; c = c->next)
    send_events_deferred(c);
  cur_time_ms = time_ms;
  m = update_sock_set(&fdset, server_fd);
  tv.tv_sec = 0;
  tv.tv_usec = 1000 * frame_ms;
  r = select(m + 1, &fdset, 0, 0, &tv);
  if (r < 0 && errno != EINTR && errno != 514)
    return -1;
  if (r > 0) {
    if (FD_ISSET(server_fd, &fdset))
      add_client(server_fd, MSIZE);
    for (c = clients; c; c = cnext) {
      cnext = c->next;
      if (FD_ISSET(c->fd, &fdset))
        if (process_client_io(c))
          rm_client(c);
    }
  }
  return 0;
}
