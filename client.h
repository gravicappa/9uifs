struct arr;
struct input_event;
struct view;

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
  char *inbuf;
  char *outbuf;

  int flags;
  struct file f;
  struct ev_pool ev;
  struct file f_views;
  struct file f_fonts;
  struct file f_comm;

  struct file *images;
  struct file *ui;

  struct ev_pool *evpools;
};

extern struct client *clients;
extern unsigned int cur_time_ms;

struct client *add_client(int server_fd, int msize);
void rm_client(struct client *c);
int client_send_resp(struct client *c);

void client_input_event(struct input_event *ev);
int draw_clients();

int process_clients(int server_fd, unsigned int time_ms,
                    unsigned int frame_ms);
