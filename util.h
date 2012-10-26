enum log_masks {
  LOG_ERR = 1,
  LOG_CLIENT = 2,
  LOG_DATA = 4,
  LOG_DBG = 8,
  LOG_MSG = 16,
  LOG_UI = 32,
};

#define containerof(ptr, type, member) \
  ((type *)((char *)ptr - offsetof(type, member)))

extern int logmask;

void die(char *fmt, ...);
void log_printf(int mask, char *fmt, ...);
void log_print_data(int mask, unsigned int size, unsigned char *buf);

struct arr {
  unsigned int used;
  unsigned int size;
  char b[sizeof(int)];
};

int arr_memcpy(struct arr **a, int delta, int off, int size,
               const void *data);
int arr_add(struct arr **a, int delta, int size, const void *data);
int arr_delete(struct arr **a, unsigned int off, unsigned int size);

char *strnchr(const char *s, unsigned int len, char c);
char *next_arg(char **s);
char *next_quoted_arg(char **s);
char *trim_string_right(char *s, char *chars);
