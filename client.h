enum {
  CLIENT_LOCAL = 1,
  CLIENT_WM = 2,
};

struct client {
  struct p9_connection con;
  struct client *next;
  struct client *prev;

  struct fid_pool fids;

  struct arr *flushed;
  struct arr *deferred;
  int fd;
  int off;
  int read;
  int size;
  unsigned char *inbuf;
  unsigned char *outbuf;

  int flags;
  struct file f;
  struct file *fonts;
  struct file *bus;
  struct file *images;
  struct file *ui;
};

extern struct client *clients;
extern unsigned int cur_time_ms;
extern struct client *wm_client;

struct client *add_client(int fd, int msize);
int client_send_resp(struct client *c);
void set_client_name(int len, char *buf, struct client *c);
struct client *client_by_id(unsigned long long id);
