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
};

struct surface *mk_surface(int w, int h);
int init_surface(struct surface *s, int w, int h);
int resize_surface(struct surface *s, int w, int h);
