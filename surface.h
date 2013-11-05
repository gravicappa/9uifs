struct surface {
  struct file f;
  struct ctl_file f_ctl;
  struct file f_size;
  struct file f_pixels;
  struct file f_format;
  struct file f_png;
  unsigned int w;
  unsigned int h;
  UImage img;
  int flags;
  struct surface_link *links;
  struct file *imglib;
};

struct surface_link {
  struct surface_link *next;
  void (*rm)(void *ptr);
  void (*update)(void *ptr, int *rect);
  void *ptr;
};

struct surface *mk_surface(int w, int h, struct file *imglib);
int init_surface(struct surface *s, int w, int h, struct file *imglib);
int resize_surface(struct surface *s, int w, int h);
void rm_surface(struct file *f);

struct surface_link *link_surface(struct surface *s, void *aux);
void unlink_surface(struct surface *s, void *aux);
