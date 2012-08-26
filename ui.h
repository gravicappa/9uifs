struct client;
struct view;

int ui_init_ui(struct client *c);
int ui_init_uiplace(struct view *v);
void ui_free();

void ui_keyboard(struct view *v, int type, int keysym, int mod,
                 unsigned int unicode);
void ui_pointer_move(struct view *v, int x, int y, int state);
void ui_pointer_click(struct view *v, int x, int y, int btn);

void ui_update_view(struct view *v);
void ui_redraw_view(struct view *v);
void ui_update();
