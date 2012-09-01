#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <Imlib2.h>

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
#include "fstypes.h"
#include "geom.h"
#include "event.h"
#include "client.h"
#include "net.h"
#include "ctl.h"
#include "draw.h"
#include "surface.h"
#include "prop.h"
#include "ui.h"
#include "view.h"
#include "config.h"

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
  if (fd < 0)
    return 0;
  log_printf(LOG_CLIENT, "# Incoming connection (fd: %d)\n", fd);
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

  memset(c->buf, 0xff, msize);

  c->fs.name = "/";
  c->fs.mode = 0500 | P9_DMDIR;
  c->fs.qpath = new_qid(FS_ROOT);

  c->ev.f.name = "event";
  init_event(&c->ev);
  add_file(&c->fs, &c->ev.f);

  c->fs_views.name = "views";
  c->fs_views.mode = 0700 | P9_DMDIR;
  c->fs_views.qpath = new_qid(FS_VIEWS);
  c->fs_views.fs = &fs_views;
  add_file(&c->fs, &c->fs_views);

  c->fs_images.name = "images";
  c->fs_images.mode = 0700 | P9_DMDIR;
  c->fs_images.qpath = new_qid(FS_IMAGES);
  add_file(&c->fs, &c->fs_images);

  c->fs_fonts.name = "fonts";
  c->fs_fonts.mode = 0700 | P9_DMDIR;
  c->fs_fonts.qpath = new_qid(FS_NONE);
  add_file(&c->fs, &c->fs_fonts);

  c->fs_comm.name = "comm";
  c->fs_comm.mode = 0700 | P9_DMDIR;
  c->fs_comm.qpath = new_qid(FS_NONE);
  add_file(&c->fs, &c->fs_comm);

  if (ui_init_ui(c) == 0) {
    c->ui->name = "ui";
    add_file(&c->fs, c->ui);
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
  log_printf(LOG_CLIENT, "; rm_client %p\n", c);

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
  free_fids(&c->fids);
  rm_file(&c->fs);
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
    log_printf(LOG_DATA, "; <- ");
    log_print_data(10, c->read, (unsigned char *)c->inbuf);
    size = unpack_uint4((unsigned char *)c->inbuf);
    log_printf(LOG_DATA, ";   size: %d\n", size);
    if (size < 7 || size > c->c.msize)
      return -1;
    log_printf(LOG_DATA, ";   c->read: %d\n", c->read);
    if (size > c->read)
      return 0;

    if (p9_unpack_msg(c->c.msize, c->inbuf, &c->c.t))
      return -1;
    if (logmask & LOG_MSG)
      p9_print_msg(&c->c.t, "<<");

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
  if (logmask & LOG_MSG)
    p9_print_msg(&c->c.r, ">>");
  outsize = unpack_uint4((unsigned char *)c->outbuf);
  log_printf(LOG_DATA, "; -> ");
  log_print_data(LOG_DATA, outsize, (unsigned char *)c->outbuf);
  if (send(c->fd, c->outbuf, outsize, 0) <= 0)
    return -1;
  return 0;
}

void
client_keyboard(int type, int keysym, int mod, unsigned int unicode)
{
  char buf[64];
  int len;

  if (!selected_view)
    return;
  len = snprintf(buf, sizeof(buf), "%u %u %u %u\n", type, keysym, mod,
                 unicode);
  put_event(selected_view->c, &selected_view->ev_keyboard, len, buf);
  ui_keyboard(selected_view, type, keysym, mod, unicode);
}

void
client_pointer_move(int x, int y, int state)
{
  char buf[48];
  int len;

  if (!selected_view)
    return;
  len = snprintf(buf, sizeof(buf), "m %u %u %u %u\n", 0, x, y, state);
  put_event(selected_view->c, &selected_view->ev_pointer, len, buf);
  ui_pointer_move(selected_view, x, y, state);
}

void
client_pointer_press(int type, int x, int y, int btn)
{
  char buf[48];
  int len;

  if (!selected_view)
    return;
  len = snprintf(buf, sizeof(buf), "%u %u %u %u %u\n", type, 0, x, y, btn);
  put_event(selected_view->c, &selected_view->ev_pointer, len, buf);
  ui_pointer_press(selected_view, type, x, y, btn);
}

static int
update_views(struct client *c)
{
  struct file *vf;
  struct view *v;
  int changed = 0;

  for (vf = c->fs_views.child; vf; vf = vf->next) {
    v = (struct view *)vf;
    if ((v->flags & VIEW_IS_DIRTY) && (v->flags & VIEW_IS_VISIBLE)) {
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

  for (vf = c->fs_views.child; vf; vf = vf->next) {
    v = (struct view *)vf;
    if (v->flags & VIEW_IS_VISIBLE)
      changed |= draw_view((struct view *)vf);
  }
  return changed;
}

int
draw_clients()
{
  struct client *c;
  int changed = 0;

  for (c = clients; c; c = c->next)
    update_views(c);
  for (c = clients; c; c = c->next)
    changed |= draw_views(c);
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
  if (r < 0)
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
