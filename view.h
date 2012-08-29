#define VIEW_TYPE_SIZE 16

enum viewflags {
  VIEW_IS_DIRTY = (1 << 0),
  VIEW_IS_VISIBLE = (1 << 1),
  VIEW_KBD_EV = (1 << 2),
  VIEW_PRESS_PTR_EV = (1 << 3),
  VIEW_MOVE_PTR_EV = (1 << 4),
};

struct view {
  struct file fs;

  struct client *c;
  char type[VIEW_TYPE_SIZE];
  int flags;
  struct surface blit;
  struct prop_rect g;

  struct ev_pool ev;
  struct ev_pool ev_pointer;
  struct ev_pool ev_keyboard;

  struct file fs_gl;
  struct file fs_canvas;
  struct file fs_visible;
  struct file *uiplace;
  struct file *uisel;
  struct file *uipointed;
};

extern struct p9_fs fs_views;

struct view *mk_view(int x, int y, int w, int h);
void moveresize_view(struct view *v, int x, int y, int w, int h);
void draw_view(struct view *v);
void update_view(struct view *v);
