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

struct uiprop {
  struct file fs;
  struct uiobj *obj;
  void (*update)(struct uiprop * self);
};

struct uiprop_int {
  struct uiprop p;
  long i;
};

struct uiprop_buf {
  struct uiprop p;
  Arr buf;
};

struct uiobj_place;

struct uiobj {
  Arr type;
  struct view *v;

  struct uiobj_place *places;

  struct uiprop_int bg;
  struct uiprop_int visible;
  struct uiprop_int drawable;
  struct uiprop_int minwidth;
  struct uiprop_int maxwidth;
  struct uiprop_int minheight;
  struct uiprop_int maxheight;

  struct file fs;
  struct file fs_evfilter;
  struct file fs_type;
  struct file fs_g;
  struct file fs_parents;

  void (*draw)(struct uiobj * place);
  void (*resize)(struct uiobj * place);
  void (*update_size)(struct uiobj * place);

  int flags;
  struct rect g;
  int req_w;
  int req_h;
  void *data;
};

struct uiobj_place {
  struct uiobj_place *next;
  struct uiobj_place *prev;
  struct uiobj *obj;
  struct uiprop_buf item;
  struct uiprop_int padx;
  struct uiprop_int pady;
  struct uiprop_buf sticky;

  struct file fs;
  struct file fs_place;

  void (*detach)(struct uiobj_place * self);

  /* temporary */
  struct uiobj_place *parent;
};

struct uiobj_container {
  struct file fs_items;
};

struct uiobj *mk_uiobj();
struct file *mk_ui(char *name);

void update_placement(struct uiobj *u);

int ui_init_prop_colour(struct uiobj *u, struct uiprop_int *p, char *name,
                        unsigned int rgba);
int ui_init_prop_int(struct uiobj *u, struct uiprop_int *p, char *name,
                     int x);
int ui_init_prop_buf(struct uiobj *u, struct uiprop_buf *p, char *name,
                     int size, char *x);
int ui_init_prop_str(struct uiobj *u, struct uiprop_buf *p, char *name,
                     int size, char *x);
