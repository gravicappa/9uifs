#define VIEW_TYPE_SIZE 32

enum viewflags {
  VIEW_IS_DIRTY = 1,
  VIEW_IS_VISIBLE = 2
};

struct view {
  struct file fs;

  struct view *next;
  struct client *c;
  struct rect g;
  char type[VIEW_TYPE_SIZE];
  int flags;
  struct surface blit;

  struct ev_pool ev;
  struct ev_pool ev_pointer;
  struct ev_pool ev_keyboard;

  struct file fs_visible;
  struct file fs_geometry;
  struct file fs_gl;
  struct file fs_canvas;
  struct file ui_place;
};

extern struct p9_fs fs_views;

struct view *mk_view(int x, int y, int w, int h);
void moveresize_view(struct view *v, int x, int y, int w, int h);
void draw_view(struct view *v);
