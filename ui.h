struct client;
struct view;
struct input_event;

int ui_init_ui(struct client *c);
int ui_init_uiplace(struct view *v);
void ui_free();

int ui_keyboard(struct view *v, struct input_event *ev);
int ui_pointer_event(struct view *v, struct input_event *ev);

void ui_update_view(struct view *v);
int ui_redraw_view(struct view *v);
int ui_update();
