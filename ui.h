struct input_event;
struct client;
struct uiobj;

struct uiobj_maker {
  char *type;
  int (*init)(struct uiobj *);
};

extern struct uiplace *ui_desktop;
extern struct uiobj *ui_focused;
extern struct uiobj *ui_pointed;
extern struct uiobj *ui_grabbed;
extern struct uiobj *ui_update_list;

struct file *mk_ui(const char *name);
void ui_free(void);
void ui_set_desktop(struct uiobj *u);

void ui_intersect_clip(int *r, int *c1, int *c2);
struct uiobj *find_uiobj(char *filename, struct client *c);
int uiobj_path(struct uiobj *u, int size, char *buf, struct client *c);
void wm_grab_key(unsigned int key, unsigned int mod);
void wm_ungrab_key(unsigned int key, unsigned int mod);
void wm_ungrab_keys(void);
