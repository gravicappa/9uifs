#include <string.h>
#include <stdlib.h>

#include "9p.h"
#include "util.h"
#include "fs.h"
#include "client.h"
#include "event.h"

static int buf_delta = 512;

void
put_event(struct client *c, struct file *f, int len, char *ev)
{
  struct ev_pool *pool;
  struct ev_listener *lsr;

  pool = (struct ev_pool *)f->aux.p;
  if (!pool)
    return;

  for (lsr = pool->listeners; lsr; lsr = lsr->next) {
    if (add_data(&lsr->buf, len, ev))
      return;
    if (lsr->tag != P9_NOTAG) {
      c->c.r.tag = lsr->tag;
      c->c.r.type = P9_RREAD;
      c->c.r.count = lsr->count;
      c->c.r.data = c->buf;
      if (c->c.r.count > lsr->buf.used)
        c->c.r.count = lsr->buf.used;
      memcpy(c->buf, lsr->buf.b, c->c.r.count);
      if (client_send_resp(c))
        return;
      rm_data(&lsr->buf, c->c.r.count, lsr->buf.b);
      lsr->tag = P9_NOTAG;
    }
  }
}

void
event_rm_fid(struct p9_fid *fid)
{
  struct file *f = (struct file *)fid->file;
  struct ev_pool *pool = (struct ev_pool *)f->aux.p;
  struct ev_listener *lsr = (struct ev_listener *)fid->aux, *p;

  if (lsr == pool->listeners)
    pool->listeners = pool->listeners->next;
  else {
    for (p = pool->listeners; p && p->next != lsr; p = p->next) {}
    if (p)
      p->next = lsr->next;
  }
  fid->aux = 0;
  fid->rm = 0;
  if (lsr->buf.b)
    free(lsr->buf.b);
  free(lsr);
}

void
event_open(struct p9_connection *c)
{
  struct ev_listener *lsr;
  struct p9_fid *fid = c->t.pfid;
  struct file *f = (struct file *)fid->file;
  struct ev_pool *pool = (struct ev_pool *)f->aux.p;

  lsr = (struct ev_listener *)malloc(sizeof(struct ev_listener));
  if (!lsr) {
    P9_SET_STR(c->r.ename, "out of memory");
    return;
  }
  memset(lsr, 0, sizeof(*lsr));
  lsr->buf.delta = buf_delta;
  lsr->next = pool->listeners;
  pool->listeners = lsr;
  lsr->tag = P9_NOTAG;
  fid->aux = lsr;
  fid->rm = event_rm_fid;
}

void
event_read(struct p9_connection *c)
{
  struct ev_listener *lsr = (struct ev_listener *)c->t.pfid->aux;
  struct client *cl = (struct client *)c;

  log_printf(3, "# event_read (buf: %u)\n", lsr->buf.used);

  if (!lsr->buf.used) {
    if (lsr->tag != P9_NOTAG) {
      P9_SET_STR(c->r.ename, "fid is already blocked on event");
      return;
    }
    lsr->tag = c->t.tag;
    lsr->count = c->t.count;
    c->r.deferred = 1;
    return;
  }
  c->r.count = (c->t.count < lsr->buf.used) ? c->t.count : lsr->buf.used;
  c->r.data = cl->buf;
  memcpy(c->r.data, lsr->buf.b, c->r.count);
  rm_data(&lsr->buf, c->r.count, lsr->buf.b);
}

void
event_flush(struct p9_connection *c)
{
  struct ev_listener *lsr = (struct ev_listener *)c->t.pfid->aux;
  if (!lsr)
    return;
  if (lsr->tag != c->t.oldtag)
    return;
  lsr->tag = P9_NOTAG;
}

struct p9_fs fs_event = {
  .open = event_open,
  .read = event_read,
  .flush = event_flush
};
