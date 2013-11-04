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
#include "config.h"

const char bus_ch_all[] = "all";
const char bus_ch_ui[] = "ui";
const char bus_ch_ptr[] = "pointer";
const char bus_ch_kbd[] = "kbd";

static int buf_delta = 512;

struct bus;
struct bus_listener;

struct bus_channel {
  struct file f;
  struct file out;
  struct file in;
  struct bus *bus;
  struct bus_listener *listeners;
};

struct bus {
  struct file f;
  struct bus_channel *channels;
  struct client *client;
  unsigned int min_time_ms;
  unsigned int nlisteners;

  struct bus_channel all;
  struct bus_channel ui;
  struct bus_channel ptr;
  struct bus_channel kbd;
};

struct bus_listener {
  struct bus_listener *next;
  struct bus_channel *chan;
  unsigned short tag;
  unsigned short flags;
  unsigned int time_ms;
  unsigned int count;
  struct arr *buf;
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
    return (abs(ev->x.i) <= 99999) ? 5 : 10;
  return sprintf(buf, "%u", ev->x.u);
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
  if (!(lsr->buf && lsr->buf->used > 0))
    return;
  c->con.r.tag = lsr->tag;
  c->con.r.type = P9_RREAD;
  c->con.r.count = lsr->count;
  c->con.r.data = c->con.buf;
  if (c->con.r.count > lsr->buf->used)
    c->con.r.count = lsr->buf->used;
  memcpy(c->con.buf, lsr->buf->b, c->con.r.count);
  if (client_send_resp(c))
    return;
  arr_delete(&lsr->buf, 0, c->con.r.count);
  lsr->tag = P9_NOTAG;
  lsr->time_ms = cur_time_ms;
}

struct bus_channel *
find_channel(struct bus *b, const char *channel)
{
  struct bus_channel *chan;

  if (channel == bus_ch_all)
    return &b->all;
  if (channel == bus_ch_ptr)
    return &b->ptr;
  if (channel == bus_ch_kbd)
    return &b->kbd;
  if (channel == bus_ch_ui)
    return &b->ui;
  for (chan = b->channels; chan; chan = (struct bus_channel *)chan->f.next)
    if (!strcmp(chan->f.name, channel))
      return chan;
  return 0;
}

void
put_event_str(struct file *bus, const char *channel, int len, char *ev)
{
  struct bus *b = (struct bus *)bus;
  struct bus_channel *chan;
  struct bus_listener *lsr;
  unsigned int t = b->min_time_ms;

  chan = find_channel(b, channel);
  if (!chan)
    return;
  for (lsr = chan->listeners; lsr; lsr = lsr->next) {
    if (arr_memcpy(&lsr->buf, buf_delta, -1, len, ev))
      return;
    if (lsr->tag != P9_NOTAG && cur_time_ms - lsr->time_ms > t)
      send_event(b->client, lsr);
  }
}

void
put_event(struct file *bus, const char **channels, struct ev_arg *ev)
{
  char buf[256], *b = buf;
  int n, i, j;

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
  for (; *channels; ++channels)
    put_event_str(bus, *channels, j, b);
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
  if (lsr->buf)
    free(lsr->buf);
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
  struct bus_channel *chan = containerof(fid->file, struct bus_channel, out);

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

void
out_read(struct p9_connection *con)
{
  struct bus_listener *lsr = (struct bus_listener *)con->t.pfid->aux;

  if (!(lsr->buf && lsr->buf->used)) {
    if (lsr->tag != P9_NOTAG) {
      P9_SET_STR(con->r.ename, "fid is already blocked on event");
      return;
    }
    lsr->tag = con->t.tag;
    lsr->count = con->t.count;
    con->r.deferred = 1;
    return;
  }
  lsr->time_ms = cur_time_ms;
  con->r.count = (con->t.count < lsr->buf->used)
      ? con->t.count : lsr->buf->used;
  con->r.data = con->buf;
  memcpy(con->r.data, lsr->buf->b, con->r.count);
  arr_delete(&lsr->buf, 0, con->r.count);
}

void
out_flush(struct p9_connection *con)
{
  struct bus_listener *lsr = (struct bus_listener *)con->t.pfid->aux;
  if (!lsr)
    return;
  if (lsr->tag != con->t.oldtag)
    return;
  lsr->tag = P9_NOTAG;
}

static void
channel_rm(struct file *f)
{
  struct bus_channel *chan = (struct bus_channel *)f;
  struct bus_listener *lsr;
  int n;
  if (chan) {
    for (n = 0, lsr = chan->listeners; lsr; lsr = lsr->next, ++n) {}
    chan->bus->nlisteners -= n;
  }
}

static struct p9_fs fs_in = {
};

static struct p9_fs fs_out = {
  .open = out_open,
  .read = out_read,
  .flush = out_flush
};

static void
channel_create(struct p9_connection *con)
{
  struct bus *bus = (struct bus *)con->t.pfid->file;
  struct bus_channel *chan;
  char *name;
  if (!(con->t.perm & P9_DMDIR)) {
    P9_SET_STR(con->r.ename, "Wrong image create permissions");
    return;
  }
  name = strndup(con->t.name, con->t.name_len);
  if (!name) {
    P9_SET_STR(con->r.ename, "Cannot allocate memory");
    return;
  }
  chan = calloc(1, sizeof(struct bus_channel));
  if (!chan) {
    free(name);
    P9_SET_STR(con->r.ename, "Cannot allocate memory");
    return;
  }
  chan->bus = bus;
  chan->f.rm = channel_rm;
  chan->f.name = name;
  chan->f.owns_name = 1;
  chan->f.mode = 0700 | P9_DMDIR;
  chan->f.qpath = new_qid(FS_BUS_CHAN);
  chan->in.name = "in";
  chan->in.mode = 0600;
  chan->in.qpath = new_qid(FS_BUS_CHAN_IN);
  chan->in.fs = &fs_in;
  chan->out.name = "out";
  chan->out.mode = 0400;
  chan->out.qpath = new_qid(FS_BUS_CHAN_OUT);
  chan->out.fs = &fs_out;
  add_file(&chan->f, &chan->in);
  add_file(&chan->f, &chan->out);
  add_file(&bus->f, &chan->f);
}

static struct p9_fs fs_bus = {
  .create = channel_create
};

static void
init_static_channel(const char *name, struct bus_channel *chan, struct bus *b)
{
  chan->f.name = (char *)name;
  chan->f.mode = 0500 | P9_DMDIR;
  chan->f.qpath = new_qid(FS_BUS_CHAN);
  chan->out.name = "out";
  chan->out.mode = 0700;
  chan->out.qpath = new_qid(FS_BUS_CHAN_OUT);
  chan->out.fs = &fs_out;
  chan->bus = b;
  add_file(&chan->f, &chan->out);
  add_file(&b->f, &chan->f);
}

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
    bus->client = c;
    bus->min_time_ms = DEF_EVENT_MIN_TIME_MS;

    init_static_channel(bus_ch_all, &bus->all, bus);
    init_static_channel(bus_ch_ui, &bus->ui, bus);
    init_static_channel(bus_ch_ptr, &bus->ptr, bus);
    init_static_channel(bus_ch_kbd, &bus->kbd, bus);
  }
  return &bus->f;
}

void
send_events_deferred(struct file *bus)
{
  struct bus *b = (struct bus *)bus;
  struct bus_channel *chan;
  struct bus_listener *lsr;
  unsigned int t, n = b->nlisteners;
  t = b->min_time_ms;

  for (chan = (struct bus_channel *)b->f.child;
       chan && n;
       chan = (struct bus_channel *)chan->f.next) {
    for (lsr = chan->listeners; lsr && n; lsr = lsr->next, --n)
      if (lsr->tag != P9_NOTAG && cur_time_ms - lsr->time_ms > t)
        send_event(b->client, lsr);
  }
}
