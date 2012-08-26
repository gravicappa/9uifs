struct arr;

struct prop {
  struct file fs;
  void (*update)(struct prop *self);
  void *aux;
};

struct prop_int {
  struct prop p;
  int i;
};

struct prop_intarr {
  struct prop p;
  int n;
  int *arr;
};

struct prop_rect {
  struct prop p;
  int r[4];
};

struct prop_buf {
  struct prop p;
  struct arr *buf;
};

int init_prop_colour(struct file *root, struct prop_int *p, char *name,
                     unsigned int rgba, void *aux);
int init_prop_int(struct file *root, struct prop_int *p, char *name, int x,
                  void *aux);
int init_prop_buf(struct file *root, struct prop_buf *p, char *name, int size,
                  char *x, int fixed_size, void *aux);
int init_prop_rect(struct file *root, struct prop_rect *p, char *name,
                   void *aux);
int init_prop_intarr(struct file *root, struct prop_intarr *p, char *name,
                     int n, int *arr, void *aux);

void prop_int_open(struct p9_connection *c, int size, const char *fmt);
int prop_int_clunk(struct p9_connection *c, const char *fmt);
void prop_int10_open(struct p9_connection *c);
void prop_int10_read(struct p9_connection *c);
void prop_int10_write(struct p9_connection *c);
void prop_int10_clunk(struct p9_connection *c);
void prop_buf_open(struct p9_connection *c);
void prop_buf_read(struct p9_connection *c);
void prop_fixed_buf_write(struct p9_connection *c);
void prop_buf_write(struct p9_connection *c);
void prop_clunk(struct p9_connection *c);
void prop_colour_open(struct p9_connection *c);
void prop_colour_read(struct p9_connection *c);
void prop_colour_write(struct p9_connection *c);
void prop_colour_clunk(struct p9_connection *c);
