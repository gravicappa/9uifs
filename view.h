#define VIEW_TYPE_SIZE 32

struct view {
  struct client *c;
  struct rect g;
  char type[VIEW_TYPE_SIZE];
  int visible;
  struct surface blit;
  struct view *next;

  struct ev_pool ev;
  struct ev_pool ev_pointer;
  struct ev_pool ev_keyboard;

  struct file fs;
  struct file fs_event;
  struct file fs_pointer;
  struct file fs_keyboard;
  struct file fs_visible;
  struct file fs_geometry;
  struct file fs_blit;
  struct file fs_gl;
  struct file fs_canvas;
  struct file fs_ui;
};

extern struct p9_fs fs_views;

struct view *mk_view(int x, int y, int w, int h);
void moveresize_view(struct view *v, int x, int y, int w, int h);
void draw_view(struct view *v);
