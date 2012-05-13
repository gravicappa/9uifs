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

struct uiobj_place;

struct uiobj {
  struct file fs;

  struct prop_buf type;
  struct prop_int bg;
  struct prop_int visible;
  struct prop_int drawable;
  struct prop_rect restraint;

  struct file fs_evfilter;
  struct file fs_g;

  /* files with aux -> uiobj_place and value with place's parent's path */
  struct file fs_places;

  void (*draw)(struct uiobj *u);
  void (*resize)(struct uiobj *u);
  void (*update_size)(struct uiobj *u);

  int flags;
  int frame_id;
  struct rect g;
  int req_w;
  int req_h;
  void *data;
};

struct uiobj_place {
  struct file fs;
  struct file fs_parent;

  struct uiobj_place *next;
  struct uiobj_place *prev;
  struct uiobj *obj;
  struct prop_buf path;
  struct prop_buf sticky;
  struct prop_rect padding;
  struct prop_rect place;

  struct file fs_place;

  void (*detach)(struct uiobj_place *self);

  /* temporary */
  struct uiobj_place *parent;
};

struct uiobj_parent {
  struct uiobj_parent *prev;
  struct uiobj_parent *next;
  struct uiobj_place *place;
};

struct uiobj_container {
  struct file fs_items;
};

struct uiobj *mk_uiobj();
void rm_uiobj(struct file *f);
struct file *mk_ui(char *name, void *aux);

void update_placement(struct uiobj *u);

void init_container_items(struct uiobj_container *c, char *name);
void update_uiobj(struct uiobj *u);
void redraw_uiobj(struct uiobj *u);

#define UIOBJ_CLIENT(u) ((struct client *)(u)->fs.aux.p)
