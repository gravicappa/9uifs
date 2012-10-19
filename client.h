struct client {
  struct p9_connection c;
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
  char *buf;

  int flags;
  struct file f;
  struct ev_pool ev;
  struct file f_views;
  struct file f_images;
  struct file f_fonts;
  struct file f_comm;

  struct file *ui;

  struct view *selected_view;
};

extern struct client *clients;
extern struct view *selected_view;
extern int framecnt[2];
extern unsigned int cur_time_ms;

#define FRAMECNT_EQ(x) ((x)[0] == framecnt[0] && (x)[1] == framecnt[1])
#define FRAMECNT_SET(x) (((x)[0] = framecnt[0]), ((x)[1] = framecnt[1]))

struct client *add_client(int server_fd, int msize);
void rm_client(struct client *c);
int client_send_resp(struct client *c);

struct input_event;

void client_input_event(struct input_event *ev);
int draw_clients();

int process_clients(int server_fd, unsigned int time_ms,
                    unsigned int frame_ms);
