struct surface {
  struct file fs;
  struct file fs_size;
  struct file fs_pixels;
  struct file fs_format;
  unsigned int w;
  unsigned int h;
  int *pixels;
  void *handle;
};

struct surface *mk_surface(int w, int h);
int init_surface(struct surface *s, int w, int h);
