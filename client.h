struct client {
  struct p9_connection c;
  int fd;
  int read;
  int size;
  int msize;
  char *inbuf;
  char *outbuf;
};

extern struct buf clients;

struct client *add_client(int server_fd, int msize);
void rm_client(struct client *c);
int process_client(struct client *c);
