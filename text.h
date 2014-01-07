int utf8_nextchar(int i, int d, int len, char *s);
int utf8_index_from_off(int off, int len, char *s);
int off_from_utf8_index(int idx, int len, char *s);

void multi_draw_utf8(UImage dst, int x, int y, int c, UFont font, int len,
                     char *s);
void multi_get_utf8_size(UFont font, int len, char *s, int *w, int *h);
int multi_get_utf8_info_at_point(UFont font, int len, char *s, int x, int y,
                                 int *cx, int *cy, int *cw, int *ch);
void multi_get_utf8_info_at_index(UFont font, int len, char *s, int idx,
                                  int *cx, int *cy, int *cw, int *ch);
int multi_index_vrel(UFont font, int len, char *s, int idx, int dir);

