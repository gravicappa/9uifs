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

void multi_draw_utf8(Image dst, int x, int y, int c, Font font, int len,
                     char *str);
void multi_get_utf8_size(Font font, int len, char *str, int *w, int *h);
