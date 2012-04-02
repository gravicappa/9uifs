enum uisticky {
  UI_STICK_START = 1,
  UI_STICK_END = 2,
};

struct uiplacement {
  int pos;
  int span;
  enum uisticky sticky;
};

enum uiprop_type {
  UI_PROP_INT = 'i',
  UI_PROP_BUF = 'b',
  UI_PROP_STR = 's',
  UI_PROP_PTR = 'p'
};

struct uiprop {
  struct file fs;
  struct uiobj *obj;
  enum uiprop_type type;
  union {
    int i;
    struct buf buf;
    void *p;
  } d;
};

struct uiobj {
  int type; /* should it be a string? */
  struct view *v;
  struct uiobj *parent;
  struct uiobj *next;
  struct uiobj *prev;
  struct uiobj *child;
  struct uiprop bg;
  struct uiprop visible;
  struct uiprop drawable;
  struct uiprop minwidth;
  struct uiprop maxwidth;
  struct uiprop minheight;
  struct uiprop maxheight;
  struct uiprop padx;
  struct uiprop pady;
  struct uiprop sticky;
  struct rect g;

  void (*draw)(struct uiobj *self);
  void (*update_size)(struct uiobj *self);

  struct file fs;
  struct file fs_evfilter;
  struct file fs_type;
  struct file fs_g;
  struct file fs_items;
};

struct uiobj *mk_uiobj();
struct uiobj *mk_ui(struct file *root, char *name, void *aux);

void update_placement(struct uiobj *u);

int ui_init_prop_colour(struct uiobj *u, struct uiprop *p, char *name, int x);
int ui_init_prop_int(struct uiobj *u, struct uiprop *p, char *name, int x);
int ui_init_prop_buf(struct uiobj *u, struct uiprop *p, char *name, int size,
                     char *x);
int ui_init_prop_str(struct uiobj *u, struct uiprop *p, char *name, int size,
                     char *x);
