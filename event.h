struct ev_listener;
struct uiobj;
struct view;
struct client;

struct ev_pool {
  struct file f;
  unsigned int min_time_ms;
  struct ev_listener *listeners;
  struct ev_pool *next;
  struct client *client;
};

struct ev_fmt {
  int (*pack)(char *buf, struct ev_fmt *fmt);
  union {
    struct uiobj *o;
    struct view *v;
    int i;
    unsigned int u;
    char *s;
  } x;
  int len;
};

int ev_int(char *buf, struct ev_fmt *ev);
int ev_uint(char *buf, struct ev_fmt *ev);
int ev_str(char *buf, struct ev_fmt *ev);

void put_event_str(struct client *c, struct ev_pool *pool, int len, char *ev);
void put_event(struct client *c, struct ev_pool *pool, struct ev_fmt *ev);
void init_event(struct ev_pool *pool, struct client *client);
void send_events_deferred(struct client *c);
