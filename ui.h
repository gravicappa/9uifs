struct client;
struct view;
struct input_event;
struct ev_fmt;

int ui_init_ui(struct client *c);
int ui_init_uiplace(struct view *v);
void ui_free();

int ui_keyboard(struct view *v, struct uiobj *u, struct input_event *ev);
int ui_pointer_event(struct view *v, struct uiobj *u, struct input_event *ev);

void ui_update_view(struct view *v);
int ui_redraw_view(struct view *v);
int ui_update();

int ev_uiobj(char *buf, struct ev_fmt *ev);
int ev_view(char *buf, struct ev_fmt *ev);
