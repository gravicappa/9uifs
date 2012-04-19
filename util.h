enum log_masks {
  LOG_CLIENT = 1,
  LOG_DATA = 2,
  LOG_DBG = 4,
  LOG_MSG = 8,
};

extern int logmask;

void die(char *fmt, ...);
void log_printf(int mask, char *fmt, ...);
void log_print_data(int mask, unsigned int size, unsigned char *buf);

typedef struct arr {
  unsigned int used;
  unsigned int size;
  char b[sizeof(int)];
} *Arr;

int arr_memcpy(Arr *a, int delta, int off, int size, const void *data);
int arr_delete(Arr *a, unsigned int off, unsigned int size);

char *strnchr(const char *s, unsigned int len, char c);
char *next_arg(char **s);
