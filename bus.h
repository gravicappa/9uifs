struct uiobj;
struct client;

extern const char bus_ch_all[];
extern const char bus_ch_ui[];
extern const char bus_ch_ptr[];
extern const char bus_ch_kbd[];

struct ev_arg {
  int (*pack)(char *buf, struct ev_arg *arg);
  union {
    struct uiobj *o;
    int i;
    unsigned int u;
    char *s;
  } x;
  int len;
};

int ev_int(char *buf, struct ev_arg *ev);
int ev_uint(char *buf, struct ev_arg *ev);
int ev_str(char *buf, struct ev_arg *ev);

void put_event(struct file *bus, const char **channels, struct ev_arg *ev);
void put_event_str(struct file *bus, const char *channel, int len, char *ev);
struct file *mk_bus(const char *name, struct client *c);
void send_events_deferred(struct file *bus);