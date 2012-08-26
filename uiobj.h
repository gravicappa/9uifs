enum uisticky {
  UI_STICK_START = 1,
  UI_STICK_END = 2,
};

struct uiplacement {
  int pos;
  int span;
  enum uisticky sticky;
};

enum uiflags {
  UI_IS_CONTAINER = 1,
  UI_IS_DIRTY = 2,
};

struct uiplace;
struct view;

struct uiobj {
  struct file fs;

  struct prop_buf type;
  struct prop_int bg;
  struct prop_int visible;
  struct prop_int drawable;
  struct prop_rect restraint;
  struct prop_rect g;

  struct file fs_evfilter;
  struct file fs_parent;

  void (*draw)(struct uiobj *u, struct view *v);
  void (*draw_over)(struct uiobj *u, struct view *v);
  void (*resize)(struct uiobj *u);
  void (*update_size)(struct uiobj *u);

  int flags;
  int reqsize[2];
  struct uiplace *parent;
  struct client *client;
  void *data;
};

struct uiplace {
  struct file fs;

  struct uiplace *next;
  struct uiplace *prev;
  struct uiobj *obj;
  struct prop_buf path;
  struct prop_buf sticky;
  struct prop_rect padding;
  struct prop_rect place;

  struct file fs_place;

  void (*detach)(struct uiplace *self);

  /* temporary */
  struct uiplace *parent;
};

struct uiobj_container {
  struct file fs_items;
  struct uiobj *u;
};

struct uiobj_maker {
  char *type;
  int (*init)(struct uiobj *);
};

struct uiobj *mk_uiobj();
void ui_rm_uiobj(struct file *f);

void ui_update_placement(struct uiobj *u);

void ui_init_container_items(struct uiobj_container *c, char *name);
void ui_update_uiobj(struct uiobj *u);
void ui_redraw_uiobj(struct uiobj *u);
void ui_update_size(struct view *v, struct uiplace *up);
void ui_place_with_padding(struct uiplace *up, int rect[4]);

void ui_propagate_dirty(struct uiplace *up);
void ui_default_prop_update(struct prop *p);
int ui_init_place(struct uiplace *up);
