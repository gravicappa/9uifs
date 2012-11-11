struct client;
struct view;
struct input_event;
struct uiobj;

extern struct view *selected_view;
extern unsigned int grab_flags;

void wm_new_view_geom(int *r);
void wm_on_create_view(struct view *v);
void wm_on_rm_view(struct view *v);
void wm_view_size_request(struct view *v);
void wm_on_input(struct input_event *ev);

void wm_grab_ptr(struct view *v, struct uiobj *u);
void wm_grab_kbd(struct view *v, struct uiobj *u);
void wm_ungrab_kbd();
void wm_ungrab_ptr();
