struct client;
struct view;

int ui_init_ui(struct client *c);
int ui_init_uiplace(struct view *v);
void ui_free();

int ui_keyboard(struct view *v, int type, int keysym, int mod,
                unsigned int unicode);
int ui_pointer_move(struct view *v, int x, int y, int state);
int ui_pointer_press(struct view *v, int type, int x, int y, int btn);

void ui_update_view(struct view *v);
int ui_redraw_view(struct view *v);
int ui_update();
