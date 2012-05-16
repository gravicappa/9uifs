struct ev_listener {
  struct ev_listener *next;
  struct file *file;
  unsigned short tag;
  unsigned int count;
  struct arr *buf;
};

struct ev_pool {
  struct file f;
  struct ev_listener *listeners;
};

struct client;

void put_event(struct client *c, struct ev_pool *pool, int len, char *ev);
void init_event(struct ev_pool *pool);
