struct image {
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
  struct image_link *links;
  struct file *imglib;
  void *conn; /* for access control */
};

struct image_link {
  struct image_link *next;
  void (*rm)(void *ptr);
  void (*update)(void *ptr, int *rect);
  void *ptr;
};

struct image *mk_image(int w, int h, struct file *imglib, void *con);
int init_image(struct image *s, int w, int h, struct file *imglib, void *con);

struct image_link *link_image(struct image *s, void *aux);
void unlink_image(struct image *s, void *aux);
