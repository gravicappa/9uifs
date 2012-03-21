extern int loglevel;

void die(char *fmt, ...);
void log_printf(int level, char *fmt, ...);
void log_print_data(int level, unsigned int size, unsigned char *buf);

struct buf {
  int delta;
  int used;
  int size;
  char *b;
};

int add_data(struct buf *buf, int size, const void *data);
int rm_data(struct buf *buf, int size, void *ptr);
char *strnchr(const char *s, unsigned int len, char c);
char *next_arg(char **s);
