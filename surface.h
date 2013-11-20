struct surface {
  struct file f;
  struct file f_size;
  struct file f_pixels;
  struct file f_format;
  struct file f_in_png;
  struct file *ctl;
  unsigned int w;
  unsigned int h;
  UImage img;
  int flags;
  struct surface_link *links;
  struct file *imglib;
  void *conn; /* for access control */
};

struct surface_link {
  struct surface_link *next;
  void (*rm)(void *ptr);
  void (*update)(void *ptr, int *rect);
  void *ptr;
};

struct surface *mk_surface(int w, int h, struct file *imglib, void *con);
int init_surface(struct surface *s, int w, int h, struct file *imglib,
                 void *con);
int resize_surface(struct surface *s, int w, int h);

struct surface_link *link_surface(struct surface *s, void *aux);
void unlink_surface(struct surface *s, void *aux);
