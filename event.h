struct ev_listener {
  struct ev_listener *next;
  struct file *file;
  unsigned short tag;
  unsigned int count;
  struct buf buf;
};

struct ev_pool {
  struct ev_listener *listeners;
};

extern struct p9_fs fs_event;

void put_event(struct client *c, struct file *f, int len, char *ev);
