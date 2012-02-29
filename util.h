extern int loglevel;

void die(char *fmt, ...);
void log_printf(int level, char *fmt, ...);
void log_print_data(int level, size_t bytes, unsigned char *buf);

struct buf {
  int delta;
  int used;
  int size;
  char *b;
};

int add_data(struct buf *buf, int bytes, const void *data);
