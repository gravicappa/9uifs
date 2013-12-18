void multi_draw_utf8(UImage dst, int x, int y, int c, UFont font, int len,
                     char *str);
void multi_get_utf8_size(UFont font, int len, char *str, int *w, int *h);
int multi_get_utf8_info_at_point(UFont font, int len, char *str, int x, int y,
                                 int *cx, int *cy, int *cw, int *ch);
void multi_get_utf8_info_at_index(UFont font, int len, char *str, int index,
                                  int *cx, int *cy, int *cw, int *ch);
