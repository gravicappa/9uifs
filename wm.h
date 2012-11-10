struct client;
struct view;

void wm_new_view_geom(int *r);
void wm_on_create_view(struct view *v);
void wm_on_rm_view(struct view *v);
void wm_view_size_request(struct view *v);
