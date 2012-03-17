struct rect {
  int x;
  int y;
  int w;
  int h;
};

struct view {
  struct rect g;
  int visible;
  struct surface blit;
  struct view *next;

  struct file fs;
  struct file fs_event;
  struct file fs_visible;
  struct file fs_geometry;
  struct file fs_blit;
  struct file fs_gl;
  struct file fs_canvas;
  struct file fs_ui;
};

extern struct p9_fs fs_views;

struct view *mk_view(int x, int y, int w, int h);
