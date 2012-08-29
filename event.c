#include <string.h>
#include <stdlib.h>

#include "9p.h"
#include "util.h"
#include "fs.h"
#include "event.h"
#include "client.h"
#include "fstypes.h"

static int buf_delta = 512;

void
put_event(struct client *c, struct ev_pool *pool, int len, char *ev)
{
  struct ev_listener *lsr;

  log_printf(LOG_DBG, ">> put_event pool: %p\n", pool);
  for (lsr = pool->listeners; lsr; lsr = lsr->next) {
    log_printf(LOG_DBG, "  lsr: %p\n", lsr);
    if (arr_memcpy(&lsr->buf, buf_delta, -1, len, ev))
      return;
    if (lsr->tag != P9_NOTAG) {
      c->c.r.tag = lsr->tag;
      c->c.r.type = P9_RREAD;
      c->c.r.count = lsr->count;
      c->c.r.data = c->buf;
      if (c->c.r.count > lsr->buf->used)
        c->c.r.count = lsr->buf->used;
      memcpy(c->buf, lsr->buf->b, c->c.r.count);
      if (client_send_resp(c))
        return;
      arr_delete(&lsr->buf, 0, c->c.r.count);
      lsr->tag = P9_NOTAG;
    }
  }
}

void
event_rm_fid(struct p9_fid *fid)
{
  struct file *f = (struct file *)fid->file;
  struct ev_pool *pool = (struct ev_pool *)f;
  struct ev_listener *lsr = (struct ev_listener *)fid->aux, *p;

  log_printf(LOG_DBG, "event_rm_fid lsr: %p\n", lsr);

  if (lsr == pool->listeners)
    pool->listeners = pool->listeners->next;
  else {
    for (p = pool->listeners; p && p->next != lsr; p = p->next) {}
    if (p)
      p->next = lsr->next;
  }
  fid->aux = 0;
  fid->rm = 0;
  if (lsr->buf)
    free(lsr->buf);
  free(lsr);
}

void
event_open(struct p9_connection *c)
{
  struct ev_listener *lsr;
  struct p9_fid *fid = c->t.pfid;
  struct file *f = (struct file *)fid->file;
  struct ev_pool *pool = (struct ev_pool *)f;

  lsr = (struct ev_listener *)malloc(sizeof(struct ev_listener));
  if (!lsr) {
    P9_SET_STR(c->r.ename, "out of memory");
    return;
  }
  memset(lsr, 0, sizeof(*lsr));
  lsr->next = pool->listeners;
  pool->listeners = lsr;
  lsr->tag = P9_NOTAG;
  fid->aux = lsr;
  fid->rm = event_rm_fid;
  log_printf(LOG_DBG, "event_open lsr: %p\n", lsr);
}

void
event_read(struct p9_connection *c)
{
  struct ev_listener *lsr = (struct ev_listener *)c->t.pfid->aux;
  struct client *cl = (struct client *)c;

  log_printf(LOG_DBG, "# event_read (buf: %u)\n",
             (lsr->buf) ? lsr->buf->used : 0);

  if (!(lsr->buf && lsr->buf->used)) {
    if (lsr->tag != P9_NOTAG) {
      P9_SET_STR(c->r.ename, "fid is already blocked on event");
      return;
    }
    lsr->tag = c->t.tag;
    lsr->count = c->t.count;
    c->r.deferred = 1;
    return;
  }
  c->r.count = (c->t.count < lsr->buf->used) ? c->t.count : lsr->buf->used;
  c->r.data = cl->buf;
  memcpy(c->r.data, lsr->buf->b, c->r.count);
  arr_delete(&lsr->buf, 0, c->r.count);
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

static void
rm_event(struct file *f)
{
  struct ev_pool *pool = (struct ev_pool *)f;
  struct ev_listener *lsr, *lsr_next;

  /* NOTE: we assume that event files cannot be deleted in runtime so living
     fids not taken care of */
  for (lsr = pool->listeners; lsr; lsr = lsr_next) {
    lsr_next = lsr->next;
    if (lsr->buf)
      free(lsr->buf);
    free(lsr);
  }
  pool->listeners = 0;
}

static struct p9_fs fs_event = {
  .open = event_open,
  .read = event_read,
  .flush = event_flush
};

void
init_event(struct ev_pool *pool)
{
  pool->f.mode = 0400;
  pool->f.qpath = new_qid(FS_EVENT);
  pool->f.fs = &fs_event;
  pool->f.rm = rm_event;
  pool->listeners = 0;
}
