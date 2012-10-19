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
#include "geom.h"
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

struct client *clients = 0;
struct client *selected_client = 0;
struct view *selected_view = 0;
unsigned int cur_time_ms;
int framecnt[2] = {0, 0};
int prevframecnt = -1;

struct client *
add_client(int server_fd, int msize)
{
  struct client *c;
  int fd;
  struct sockaddr_in addr;
  socklen_t addr_len;

  addr_len = sizeof(addr);
  fd = accept(server_fd, (struct sockaddr *)&addr, &addr_len);
  if (fd < 0 || nonblock_socket(fd))
    return 0;
  log_printf(LOG_CLIENT, "# Incoming connection (fd: %d)\n", fd);
  c = (struct client *)calloc(1, sizeof(struct client));
  if (!c)
    die("Cannot allocate memory");
  c->fd = fd;
  c->c.msize = msize;
  c->inbuf = malloc(msize);
  c->outbuf = malloc(msize);
  c->buf = malloc(msize);

  if (!(c->inbuf && c->outbuf && c->buf))
    die("Cannot allocate memory");

  memset(c->buf, 0xff, msize);

  c->f.name = "/";
  c->f.mode = 0500 | P9_DMDIR;
  c->f.qpath = new_qid(FS_ROOT);

  c->ev.f.name = "event";
  init_event(&c->ev);
  add_file(&c->f, &c->ev.f);

  c->f_views.name = "views";
  c->f_views.mode = 0700 | P9_DMDIR;
  c->f_views.qpath = new_qid(FS_VIEWS);
  c->f_views.fs = &fs_views;
  add_file(&c->f, &c->f_views);

  c->f_images.name = "images";
  c->f_images.mode = 0700 | P9_DMDIR;
  c->f_images.qpath = new_qid(FS_IMAGES);
  add_file(&c->f, &c->f_images);

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
  if (c->buf)
    free(c->buf);
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

static int
process_client_io(struct client *c)
{
  int r;
  unsigned int size, off;
  char *buf = c->inbuf;

  do {
    if (c->read >= c->c.msize - (c->c.msize >> 4)) {
      memmove(buf, buf + c->off, c->read - c->off);
      c->read -= c->off;
      c->off = 0;
    }
    r = recv(c->fd, buf + c->read, c->c.msize - c->read, 0);
    if (r == 0)
      return 0;
    if (r < 0)
      return (net_wouldblock())? 0 : -1;
    c->read += r;
    if (c->read < 4)
      return 0;
    off = c->off;
    while (c->read - off >= 4) {
      size = unpack_uint4((unsigned char *)buf + off);
      if (size < 7 || size > c->c.msize)
        return -1;
      if (off + size > c->read)
        break;
      if (p9_unpack_msg(size, buf + off, &c->c.t))
        return -1;
      c->c.r.deferred = 0;
      if (p9_process_treq(&c->c, &fs))
        return -1;
      if (!c->c.r.deferred && client_send_resp(c))
        return -1;
      off += size;
      size = 0;
    }
    c->off = off;
  } while (1);
  return 0;
}

int
client_send_resp(struct client *c)
{
  unsigned int outsize;

  if (p9_pack_msg(c->c.msize, c->outbuf, &c->c.r))
    return -1;
  outsize = unpack_uint4((unsigned char *)c->outbuf);
  if (send(c->fd, c->outbuf, outsize, 0) <= 0)
    return -1;
  return 0;
}

void
client_input_event(struct input_event *ev)
{
  char buf[48];
  int len, type;

  if (!selected_view)
    return;

  switch (ev->type) {
    case IN_PTR_MOVE:
      len = snprintf(buf, sizeof(buf), "m %u %u %u %d %d %u\n", ev->id,
                     ev->x, ev->y, ev->dx, ev->dy, ev->state);
      put_event(selected_view->c, &selected_view->ev_pointer, len, buf);
      ui_pointer_event(selected_view, ev);
      break;

    case IN_PTR_DOWN:
    case IN_PTR_UP:
      type = (ev->type == IN_PTR_DOWN) ? 1 : 0;
      len = snprintf(buf, sizeof(buf), "%u %u %u %u %u\n", type, ev->id,
                     ev->x, ev->y, ev->key);
      put_event(selected_view->c, &selected_view->ev_pointer, len, buf);
      ui_pointer_event(selected_view, ev);
      break;

    case IN_KEY_DOWN:
    case IN_KEY_UP:
      type = (ev->type == IN_KEY_DOWN) ? 1 : 0;
      len = snprintf(buf, sizeof(buf), "%u %u %u %lu\n", type, ev->key,
                     ev->state, ev->unicode);
      put_event(selected_view->c, &selected_view->ev_keyboard, len, buf);
      ui_keyboard(selected_view, ev);
      break;
  }
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

  for (c = clients; c; c = c->next) {
    update_views(c);
    changed |= draw_views(c);
  }
  changed |= ui_update();
  return changed;
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
