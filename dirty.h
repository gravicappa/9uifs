void init_dirty(int x, int y, int w, int h);
void set_dirty_base_rect(int r[4]);
void mark_dirty_rect(int r[4]);
void iterate_dirty_rects(void (*fn)(int r[4], void *aux), void *aux);
void clean_dirty_rects();
void optimize_dirty_rects();
