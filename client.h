struct client {
  struct p9_connection c;
  struct client *next;

  struct fid_pool fids;

  struct buf flushed;
  struct buf deferred;
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

  struct view *selected_view;
  struct view *views;
};

extern struct client *clients;
extern struct view *selected_view;

struct client *add_client(int server_fd, int msize);
void rm_client(struct client *c);
int process_client_io(struct client *c);
int client_send_resp(struct client *c);

void client_keyboard(int type, int keysym, int mod, unsigned int unicode);
void client_pointer_move(int x, int y, int state);
void client_pointer_click(int type, int x, int y, int btn);

void draw_views(struct client *c);
