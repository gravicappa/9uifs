extern int dirty_rects[];
extern int ndirty_rects;

void init_dirty(int x, int y, int w, int h);
void set_dirty_base_rect(int r[4]);
void add_dirty_rect(int r[4]);
void add_dirty_rect2(int x, int y, int w, int h);
void clean_dirty_rects(void);
void prepare_dirty_rects(void);
