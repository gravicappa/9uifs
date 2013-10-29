enum uiflags {
  UI_CONTAINER = (1 << 0),
  UI_DIRTY = (1 << 1),
  UI_DISABLED = (1 << 2),
  UI_KBD_EV = (1 << 3),
  UI_UPDOWN_PTR_EV = (1 << 4),
  UI_MOVE_PTR_EV = (1 << 5),
  UI_PTR_INTERSECT_EV = (1 << 6),
  UI_RESIZE_EV = (1 << 7),
  UI_SEE_THROUGH = (1 << 8),
};

struct uiplace;
struct view;
struct uiobj;
struct bus;
struct ev_arg;
struct uicontext;
struct input_event;

struct uiobj_ops {
  int allocated;
  void (*draw)(struct uiobj *u, struct uicontext *uc);
  void (*draw_over)(struct uiobj *u, struct uicontext *uc);
  void (*update)(struct uiobj *u);
  void (*resize)(struct uiobj *u);
  void (*update_size)(struct uiobj *u);
  int (*on_input)(struct uiobj *u, struct input_event *ev);
  int (*on_ptr_intersect)(struct uiobj *u, int inside);
  struct file *(*get_children)(struct uiobj *u);
};

struct uiobj {
  struct file f;
  struct uiobj *next;
  struct uiobj_ops *ops;

  struct prop_buf type;
  struct prop_int bg;
  struct prop_int visible;
  struct prop_int drawable;
  struct prop_rect restraint;
  struct prop_rect g;
  struct prop_rect viewport;

  struct file f_evfilter;
  struct file f_parent;

  int flags;
  int reqsize[2];
  struct uiplace *place;
  struct client *client;
  void *data;
};

struct uiplace {
  struct file f;
  struct uiobj *obj;

  struct prop_buf sticky;
  struct prop_rect padding;
  struct prop_rect place;

  struct file f_path;

  void (*detach)(struct uiplace *self);

  /* temporary */
  int clip[4];
  struct uiplace *parent;
};

struct uiobj_container {
  struct file f_items;
  struct uiobj *u;
};

struct uicontext {
  int dirty;
  int clip[4];
  struct uiobj *dirtyobj;
};

struct uiobj *mk_uiobj(struct client *c);
void ui_rm_uiobj(struct file *f);

void ui_update_placement(struct uiobj *u);

void ui_init_container_items(struct uiobj_container *c, char *name);
void ui_update_uiobj(struct uiobj *u);
void ui_redraw_uiobj(struct uiobj *u);
void ui_update_size(struct view *v, struct uiplace *up);
void ui_place_with_padding(struct uiplace *up, int rect[4]);

void ui_propagate_dirty(struct uiplace *up);
struct view *ui_get_uiobj_view(struct uiobj *u);
void ui_prop_update_default(struct prop *p);
void uiplace_prop_update_default(struct prop *p);
int ui_init_place(struct uiplace *up, int setup);

void default_draw_uiobj(struct uiobj *u, struct uicontext *uc);

void walk_ui_tree(struct uiplace *up,
                  int (*before_fn)(struct uiplace *, void *),
                  int (*after_fn)(struct uiplace *, void *),
                  void *aux);

int put_ui_event(struct bus *bus, struct client *c, const char *fmt, ...);
void ui_init_evfilter(struct file *f);
void ui_enqueue_update(struct uiobj *u);

struct file *uiobj_children(struct uiobj *u);
struct uiobj *uiplace_container(struct uiplace *up);

int ev_uiobj(char *buf, struct ev_arg *ev);
