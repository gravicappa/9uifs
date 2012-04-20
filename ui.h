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
};

struct uiobj_place;

struct uiobj {
  struct view *v;

  struct uiobj_place *places;

  struct prop_buf type;
  struct prop_int bg;
  struct prop_int visible;
  struct prop_int drawable;
  struct prop_int minwidth;
  struct prop_int maxwidth;
  struct prop_int minheight;
  struct prop_int maxheight;

  struct file fs;
  struct file fs_evfilter;
  struct file fs_g;
  struct file fs_parents;

  void (*draw)(struct uiobj *u);
  void (*resize)(struct uiobj *u);
  void (*update_size)(struct uiobj *u);

  int flags;
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
  struct prop_int padx;
  struct prop_int pady;

  struct file fs_place;

  void (*detach)(struct uiobj_place *self);

  /* temporary */
  struct uiobj_place *parent;
};

struct uiobj_container {
  struct file fs_items;
};

struct uiobj *mk_uiobj();
void rm_uiobj(struct file *f);
struct file *mk_ui(char *name);

void update_placement(struct uiobj *u);
