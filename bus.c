#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>

#include "9p.h"
#include "util.h"
#include "fs.h"
#include "fsutil.h"
#include "bus.h"
#include "client.h"
#include "fstypes.h"
#include "ctl.h"
#include "config.h"

#define BUS_BUF_BYTES 65536 /* must be the power of two */
#define IDX(x) ((x) & (BUS_BUF_BYTES - 1))

const char bus_ch_ev[] = "ev";
const char bus_ch_ptr[] = "pointer";
const char bus_ch_kbd[] = "kbd";

struct bus;
struct bus_listener;

struct bus_channel {
  struct file f;
  struct bus *bus;
  struct bus_listener *listeners;
};

struct bus {
  struct file f;
  struct file dir;
  struct client *client;
  unsigned int min_time_ms;
  unsigned int nlisteners;
  unsigned int ndeferred;

  struct bus_channel ev;
  struct bus_channel ptr;
  struct bus_channel kbd;
};

struct bus_listener {
  struct bus_listener *next;
  struct bus_channel *chan;
  unsigned short tag;
  unsigned short flags;
  unsigned int time_ms;
  int count;
  int off;
  int size;
  unsigned char buf[BUS_BUF_BYTES];
};

int
ev_int(char *buf, struct ev_arg *ev)
{
  if (!buf)
    return (abs(ev->x.i) <= 99999) ? 6 : 11;
  return sprintf(buf, "%d", ev->x.i);
}

int
ev_uint(char *buf, struct ev_arg *ev)
{
  if (!buf)
    return (abs(ev->x.u) <= 99999) ? 5 : 10;
  return sprintf(buf, "%u", ev->x.u);
}

int
ev_ull(char *buf, struct ev_arg *ev)
{
  if (!buf)
    return (abs(ev->x.ull) <= 99999) ? 5 : 20;
  return sprintf(buf, "%llu", ev->x.ull);
}

int
ev_str(char *buf, struct ev_arg *ev)
{
  if (!buf)
    return strlen(ev->x.s);
  return sprintf(buf, "%s", ev->x.s);
}

static void
send_event(struct client *c, struct bus_listener *lsr)
{
  int n, e, bytes;
  unsigned char *buf;

  if (!lsr->size || lsr->tag == P9_NOTAG)
    return;
  c->con.r.tag = lsr->tag;
  c->con.r.type = P9_RREAD;
  c->con.r.data = c->con.buf;

  n = bytes = (lsr->count < lsr->size) ? lsr->count : lsr->size;
  c->con.r.count = n;
  if (n) {
    buf = lsr->buf;
    e = lsr->off + n;
    if (e > sizeof(lsr->buf)) {
      e -= sizeof(lsr->buf);
      n = sizeof(lsr->buf) - lsr->off;
      memcpy(c->con.buf, buf + lsr->off, n);
      memcpy((char *)c->con.buf + n, buf, e);
      lsr->off = e;
      lsr->size -= bytes;
      c->con.r.count = e + n;
    } else {
      memcpy(c->con.buf, buf + lsr->off, e - lsr->off);
      n = e - lsr->off;
      lsr->off = e;
      lsr->size -= bytes;
      c->con.r.count = n;
    }
  }
  if (client_send_resp(c))
    return;
  lsr->chan->bus->ndeferred--;
  lsr->tag = P9_NOTAG;
  lsr->time_ms = cur_time_ms;
}

struct bus_channel *
find_channel(struct bus *b, const char *channel)
{
  struct file *f;

  if (channel == bus_ch_ev)
    return &b->ev;
  if (channel == bus_ch_ptr)
    return &b->ptr;
  if (channel == bus_ch_kbd)
    return &b->kbd;
  for (f = b->f.child; f; f = f->next)
    if (FSTYPE(*f) == FS_BUS_CHAN_OUT && !strcmp(f->name, channel))
      return (struct bus_channel *)f;
  return 0;
}

static void
write_buf(int bytes, char *data, struct bus_listener *lsr)
{
  int n, i;
  unsigned char *buf = lsr->buf;

  if (bytes > sizeof(lsr->buf)) {
    data += bytes - sizeof(lsr->buf);
    bytes = sizeof(lsr->buf);
  }
  if (!bytes)
    return;
  n = bytes;
  for (i = IDX(lsr->off + lsr->size); n; --n, i = IDX(i + 1))
    buf[i] = *data++;
  if (lsr->size + bytes > sizeof(lsr->buf)) {
    lsr->off = i;
    lsr->size = sizeof(lsr->buf);
  } else
    lsr->size += bytes;
}

static void
put_str(struct file *bus, struct bus_channel *chan, int len, char *ev)
{
  struct bus *b = (struct bus *)bus;
  struct bus_listener *lsr;
  unsigned int t = b->min_time_ms;

  for (lsr = chan->listeners; lsr; lsr = lsr->next) {
    write_buf(len, ev, lsr);
    if (lsr->tag != P9_NOTAG && cur_time_ms - lsr->time_ms > t)
      send_event(b->client, lsr);
  }
}

void
put_event_str(struct file *bus, const char *channel, int len, char *ev)
{
  struct bus_channel *chan;

  chan = find_channel((struct bus *)bus, channel);
  if (chan)
    put_str(bus, chan, len, ev);
}

void
put_event(struct file *bus, const char *channel, struct ev_arg *ev)
{
  char buf[256], *b = buf;
  int n, i, j;

  log_printf(LOG_DBG, "put_event/ nlisteners: %d\n",
             ((struct bus *)bus)->nlisteners);
  if (!(bus && ((struct bus *)bus)->nlisteners))
    return;
  for (n = i = 0; ev[i].pack; ++i) {
    ev[i].len = ev[i].pack(0, &ev[i]);
    n += ev[i].len + 1;
  }
  if (n >= sizeof(buf)) {
    b = malloc(n);
    if (!b)
      return;
  }
  for (j = i = 0; ev[i].pack; ++i) {
    j += ev[i].pack(b + j, &ev[i]);
    b[j++] = '\t';
  }
  b[j - 1] = '\n';
  put_event_str(bus, channel, j, b);
  if (b != buf)
    free(b);
}

void
event_rm_fid(struct p9_fid *fid)
{
  struct bus_listener *lsr = fid->aux, *p, **pp;
  struct bus_channel *chan = lsr->chan;
  pp = &chan->listeners;
  for (p = *pp; p && p != lsr; pp = &p->next, p = p->next) {}
  if (p)
    *pp = p->next;
  if (lsr->tag != P9_NOTAG) {
    struct client *c = chan->bus->client;
    c->con.r.tag = lsr->tag;
    c->con.r.type = P9_RREAD;
    c->con.r.data = c->con.buf;
    c->con.r.count = 0;
    client_send_resp(c);
    chan->bus->ndeferred--;
  }
  free(lsr);
  fid->aux = 0;
  fid->rm = 0;
  chan->bus->nlisteners--;
}

void
out_open(struct p9_connection *con)
{
  struct bus_listener *lsr;
  struct p9_fid *fid = con->t.pfid;
  struct bus_channel *chan = (struct bus_channel *)fid->file;

  lsr = (struct bus_listener *)calloc(1, sizeof(struct bus_listener));
  if (!lsr) {
    P9_SET_STR(con->r.ename, "Cannot allocate memory");
    return;
  }
  chan->bus->nlisteners++;
  lsr->next = chan->listeners;
  chan->listeners = lsr;
  lsr->chan = chan;
  lsr->tag = P9_NOTAG;
  lsr->time_ms = cur_time_ms;
  fid->aux = lsr;
  fid->rm = event_rm_fid;
}

static void
out_read(struct p9_connection *con)
{
  struct bus_listener *lsr = (struct bus_listener *)con->t.pfid->aux;
  struct p9_fid *fid = con->t.pfid;
  struct bus_channel *chan = (struct bus_channel *)fid->file;

  if (lsr->tag != P9_NOTAG) {
    P9_SET_STR(con->r.ename, "fid is already blocked on event");
    return;
  }
  lsr->tag = con->t.tag;
  lsr->count = con->t.count;
  con->r.deferred = 1;
  chan->bus->ndeferred++;
}

static void
out_flush(struct p9_connection *con)
{
  struct bus_listener *lsr = (struct bus_listener *)con->t.pfid->aux;
  if (!lsr)
    return;
  if (lsr->tag != con->t.oldtag)
    return;
  if (lsr->tag != P9_NOTAG)
    lsr->chan->bus->ndeferred--;
  lsr->tag = P9_NOTAG;
}

static void
rm_channel(struct file *f)
{
  struct bus_channel *chan = (struct bus_channel *)f;
  struct bus_listener *lsr;
  int n;
  if (chan) {
    for (n = 0, lsr = chan->listeners; lsr; lsr = lsr->next, ++n) {}
    chan->bus->nlisteners -= n;
  }
}

static struct p9_fs fs_out = {
  .open = out_open,
  .read = out_read,
  .flush = out_flush
};

static void
init_out(const char *name, struct bus_channel *chan, struct bus *b)
{
  chan->f.name = (char *)name;
  chan->f.mode = 0400;
  chan->f.qpath = new_qid(FS_BUS_CHAN_OUT);
  chan->f.fs = &fs_out;
  chan->bus = b;
  add_file(&b->f, &chan->f);
}

static void
channel_create(struct p9_connection *con)
{
  struct bus *bus = (struct bus *)con->t.pfid->file;
  struct bus_channel *chan;
  struct file *dir = 0;
  char *name = 0;

  P9_SET_STR(con->r.ename, "Not implemented");
  return;
  if (!(con->t.perm & P9_DMDIR)) {
    P9_SET_STR(con->r.ename, "Wrong channel create permissions");
    return;
  }
  name = strndup(con->t.name, con->t.name_len);
  if (!name)
    goto err;
  dir = calloc(1, sizeof(struct file));
  if (!dir)
    goto err;
  dir->name = (char *)name;
  dir->owns_name = 1;
  dir->mode = 0500 | P9_DMDIR;
  dir->qpath = new_qid(FS_BUS_CHAN);

  chan = calloc(1, sizeof(struct bus_channel));
  if (!chan)
    goto err;
  init_out("out", chan, bus);
  chan->f.rm = rm_channel;
  /*
  f = mk_chan_in(chan);
  if (!f)
    goto err;
    */
  return;
err:
  P9_SET_STR(con->r.ename, "Cannot create channel");
  if (name)
    free(name);
  if (dir)
    rm_file(dir);
}

static struct p9_fs fs_bus = {
  .create = channel_create
};

struct file *
mk_bus(const char *name, struct client *c)
{
  struct bus *bus;
  bus = calloc(1, sizeof(struct bus));
  if (bus) {
    bus->f.name = (char *)name;
    bus->f.mode = 0700 | P9_DMDIR;
    bus->f.qpath = new_qid(FS_BUS);
    bus->f.fs = &fs_bus;
    bus->f.rm = free_file;

    bus->client = c;
    bus->min_time_ms = DEF_EVENT_MIN_TIME_MS;

    init_out(bus_ch_ev, &bus->ev, bus);
    init_out(bus_ch_ptr, &bus->ptr, bus);
    init_out(bus_ch_kbd, &bus->kbd, bus);
  }
  return &bus->f;
}

void
send_events_deferred(struct file *bus)
{
  struct bus *b = (struct bus *)bus;
  struct bus_channel *chan;
  struct bus_listener *lsr;
  struct file *f;
  int n = b->ndeferred;
  unsigned int t;

  t = b->min_time_ms;
  for (f = b->f.child; f && n; f = f->next)
    if (FSTYPE(*f) == FS_BUS_CHAN_OUT) {
      chan = (struct bus_channel *)f;
      for (lsr = chan->listeners; lsr && n; lsr = lsr->next, --n)
        if (cur_time_ms - lsr->time_ms > t)
          send_event(b->client, lsr);
    }
}
