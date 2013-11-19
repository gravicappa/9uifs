int ui_init_place(struct uiplace *up, int setup);
void ui_create_place(struct p9_connection *c);
struct uiobj *uiplace_container(struct uiplace *up);
int uiplace_set_attach_flag(struct uiplace *up, void *aux);
int uiplace_unset_attach_flag(struct uiplace *up, void *aux);
