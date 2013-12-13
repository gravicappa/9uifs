struct uiobj;
struct client;

extern const char bus_ch_ev[];
extern const char bus_ch_ptr[];
extern const char bus_ch_kbd[];

struct ev_arg {
  int (*pack)(char *buf, struct ev_arg *arg, struct client *c);
  union {
    struct uiobj *o;
    int i;
    unsigned int u;
    unsigned long long ull;
    char *s;
  } x;
  int len;
};

int ev_int(char *buf, struct ev_arg *ev, struct client *c);
int ev_uint(char *buf, struct ev_arg *ev, struct client *c);
int ev_ull(char *buf, struct ev_arg *ev, struct client *c);
int ev_str(char *buf, struct ev_arg *ev, struct client *c);

void put_event(struct file *bus, const char *channel, struct ev_arg *ev);
void put_event_str(struct file *bus, const char *channel, int len, char *ev);
struct file *mk_bus(const char *name, struct client *c);
void send_events_deferred(struct file *bus);
