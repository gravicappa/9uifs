struct surface {
  struct file fs;
  struct ctl_file fs_ctl;
  struct file fs_size;
  struct file fs_pixels;
  struct file fs_format;
  unsigned int w;
  unsigned int h;
  Imlib_Image img;
  int size_opened;
  int pixels_opened;
};

struct surface *mk_surface(int w, int h);
int init_surface(struct surface *s, int w, int h);
