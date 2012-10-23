#include <string.h>
#include <stdlib.h>

#include "9p.h"
#include "util.h"
#include "fs.h"
#include "event.h"
#include "client.h"
#include "fstypes.h"

struct ev_listener {
  struct ev_listener *next;
  struct file *file;
  unsigned short tag;
  unsigned int time_ms;
  unsigned int count;
  struct arr *buf;
};

static int buf_delta = 512;

/* TODO: think about accumulating events within given timelimit to
  decrease IO for some cases. */

void
put_event(struct client *c, struct ev_pool *pool, int len, char *ev)
{
  struct ev_listener *lsr;

  for (lsr = pool->listeners; lsr; lsr = lsr->next) {
    if (arr_memcpy(&lsr->buf, buf_delta, -1, len, ev))
      return;
    if (lsr->tag != P9_NOTAG) {
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
      lsr->time_ms = 0;
    }
  }
}

void
event_rm_fid(struct p9_fid *fid)
{
  struct file *f = (struct file *)fid->file;
  struct ev_pool *pool = (struct ev_pool *)f;
  struct ev_listener *lsr = (struct ev_listener *)fid->aux, *p;

  if (!pool)
    return;
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

  lsr = (struct ev_listener *)calloc(1, sizeof(struct ev_listener));
  if (!lsr) {
    P9_SET_STR(c->r.ename, "out of memory");
    return;
  }
  lsr->next = pool->listeners;
  pool->listeners = lsr;
  lsr->tag = P9_NOTAG;
  fid->aux = lsr;
  fid->rm = event_rm_fid;
}

void
event_read(struct p9_connection *con)
{
  struct ev_listener *lsr = (struct ev_listener *)con->t.pfid->aux;

  if (0)
    log_printf(LOG_DBG, "# event_read (buf: %u)\n",
               (lsr->buf) ? lsr->buf->used : 0);

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
event_flush(struct p9_connection *con)
{
  struct ev_listener *lsr = (struct ev_listener *)con->t.pfid->aux;
  if (!lsr)
    return;
  if (lsr->tag != con->t.oldtag)
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
