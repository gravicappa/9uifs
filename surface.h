struct surface {
  struct file fs;
  struct ctl_file fs_ctl;
  struct file fs_size;
  struct file fs_pixels;
  struct file fs_format;
  unsigned int w;
  unsigned int h;
  Image img;
};

struct surface *mk_surface(int w, int h);
int init_surface(struct surface *s, int w, int h);
int resize_surface(struct surface *s, int w, int h);
