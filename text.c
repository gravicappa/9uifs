#include "backend.h"
#include "text.h"
#include "util.h"

void
multi_draw_utf8(UImage dst, int x, int y, int c, UFont fnt, int len, char *s)
{
  int i, j, dx, dy;

  if (!(s && len))
    return;
  for (i = j = 0; i < len; ++i)
    if (s[i] == '\n' || s[i] == '\r') {
      if (j < i) {
        draw_utf8(dst, x, y, c, fnt, i - j, s + j);
        get_utf8_size(fnt, i - j, s + j, &dx, &dy);
        y += dy;
      }
      j = i + 1;
    }
  if (j < i)
    draw_utf8(dst, x, y, c, fnt, i - j, s + j);
}

void
multi_get_utf8_size(UFont font, int len, char *str, int *w, int *h)
{
  int x, y, x1, y1, i, j;

  x = y = 0;
  if (str && len)
    for (i = j = 0; i <= len; ++i)
      if (str[i] == '\n' || str[i] == '\r' || i >= len) {
        if (j < i) {
          get_utf8_size(font, i - j, str + j, &x1, &y1);
          x = (x > x1) ? x : x1;
          y += y1;
        }
        j = i + 1;
      }
  *w = x;
  *h = y;
}
