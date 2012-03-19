struct client {
  struct p9_connection c;
  struct client *next;

  struct p9_fid *fids;
  struct p9_fid *fids_pool;

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

struct client *add_client(int server_fd, int msize);
void rm_client(struct client *c);
int process_client(struct client *c);
int client_send_resp(struct client *c);

void reset_fids(struct client *c);
struct p9_fid *get_fid(unsigned int fid, struct client *c);
struct p9_fid *add_fid(unsigned int fid, struct client *c);
void rm_fid(struct p9_fid *fid, struct client *c);
