struct text {
  int w;
  int h;
  int curpos;
  int font;
  int owns_text;
  char *text;
};

int reflow_text(struct text *t);
int offset_from_coords(struct text *t, int x, int y);

void multi_draw_utf8(UImage dst, int x, int y, int c, UFont font, int len,
                     char *str);
void multi_get_utf8_size(UFont font, int len, char *str, int *w, int *h);
