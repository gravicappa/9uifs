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

  /* files with aux -> uiplace and value with place's parent's path */
  struct file fs_places;

  void (*draw)(struct uiobj *u, struct view *v);
  void (*resize)(struct uiobj *u);
  void (*update_size)(struct uiobj *u);

  int flags;
  int frame;
  int frame1;
  int reqsize[2];
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

struct uiobj_parent {
  struct uiobj_parent *prev;
  struct uiobj_parent *next;
  struct uiplace *place;
};

struct uiobj_container {
  struct file fs_items;
};

struct uiobj *mk_uiobj();
void ui_rm_uiobj(struct file *f);

void ui_update_placement(struct uiobj *u);

void ui_init_container_items(struct uiobj_container *c, char *name);
void ui_update_uiobj(struct uiobj *u);
void ui_redraw_uiobj(struct uiobj *u);

#define UIOBJ_CLIENT(u) ((struct client *)(u)->fs.aux.p)

void ui_update_size(struct view *v, struct uiplace *up);
void ui_place_with_padding(struct uiplace *up, int rect[4]);
