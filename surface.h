struct surface {
  struct file f;
  struct file fsize;
  struct file fpixels;
  struct file fformat;
  char size_buf[32];
  unsigned int w;
  unsigned int h;
  int *pixels;
  void *handle;
};

struct surface *mk_surface(int w, int h);
