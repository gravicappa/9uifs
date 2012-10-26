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
  void (*update)(struct surface *s);
  void *aux;
  struct file *imglib;
};

struct surface *mk_surface(int w, int h, struct file *imglib);
int init_surface(struct surface *s, int w, int h, struct file *imglib);
int resize_surface(struct surface *s, int w, int h);
