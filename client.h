struct client {
  struct p9_connection c;
  struct client *next;
  struct client *prev;

  struct fid_pool fids;

  struct arr *flushed;
  struct arr *deferred;
  int fd;
  int read;
  int size;
  char *inbuf;
  char *outbuf;
  char *buf;

  struct file fs;
  struct file fs_event;
  struct file fs_views;
  struct file fs_images;
  struct file fs_fonts;
  struct file fs_comm;

  struct file *ui;

  struct view *selected_view;
};

extern struct client *clients;
extern struct view *selected_view;
extern int framecnt[2];

#define FRAMECNT_EQ(x) ((x)[0] == framecnt[0] && (x)[1] == framecnt[1])
#define FRAMECNT_SET(x) (((x)[0] = framecnt[0]), ((x)[1] = framecnt[1]))
/*
#define FRAMECNT_SET(x) \
  do { \
    log_printf(LOG_UI, "set-framecnt to %d at %s:%d\n", framecnt[0], \
               __FILE__, __LINE__); \
    ((x)[0] = framecnt[0]), ((x)[1] = framecnt[1]); \
  } while(0)
*/

struct client *add_client(int server_fd, int msize);
void rm_client(struct client *c);
int process_client_io(struct client *c);
int client_send_resp(struct client *c);

void client_keyboard(int type, int keysym, int mod, unsigned int unicode);
void client_pointer_move(int x, int y, int state);
void client_pointer_click(int type, int x, int y, int btn);
void draw_clients();
