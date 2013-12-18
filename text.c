#include "frontend.h"
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
      draw_utf8(dst, x, y, c, fnt, i - j, s + j);
      if (i != j)
        get_utf8_size(fnt, i - j, s + j, &dx, &dy);
      else
        get_utf8_size(fnt, 1, ".", &dx, &dy);
      y += dy;
      j = i + 1;
    }
  if (j < i)
    draw_utf8(dst, x, y, c, fnt, i - j, s + j);
}

void
multi_get_utf8_size(UFont font, int len, char *s, int *w, int *h)
{
  int w1, h1, i, j;

  *w = *h = 0;
  if (s && len)
    for (i = j = 0; i <= len; ++i)
      if (s[i] == '\n' || s[i] == '\r' || i >= len) {
        if (i != j)
          get_utf8_size(font, i - j, s + j, &w1, &h1);
        else
          get_utf8_size(font, 1, ".", &w1, &h1);
        *w = (*w > w1) ? *w : w1;
        *h += h1;
        j = i + 1;
      }
}

int
multi_get_utf8_info_at_point(UFont font, int len, char *s, int x, int y,
                             int *cx, int *cy, int *cw, int *ch)
{
  int i, j, w1, h1, r;
  if (!(s && len && x >= 0 && y >= 0))
    return -1;
  for (i = j = 0; i <= len; ++i)
    if (s[i] == '\n' || s[i] == '\r' || i >= len) {
      if (i != j)
        get_utf8_size(font, i - j, s + j, &w1, &h1);
      else
        get_utf8_size(font, 1, ".", &w1, &h1);
      if (y <= h1 && x <= w1) {
        r = get_utf8_info_at_point(font, i - j, s + j, x, y, cx, cy, cw, ch);
        if (r >= 0)
          return r;
      }
      y -= h1;
      if (y < 0)
        return -1;
      j = i + 1;
    }
  return -1;
}

void
multi_get_utf8_info_at_index(UFont font, int len, char *s, int index,
                             int *cx, int *cy, int *cw, int *ch)
{
  int i, j, w1, h1, y0;
  if (!(s && len && index >= 0 && index < len))
    return;
  for (y0 = i = j = 0; i < len && i <= index; ++i)
    if (s[i] == '\n' || s[i] == '\r') {
      if (i != j)
        get_utf8_size(font, i - j, s + j, &w1, &h1);
      else
        get_utf8_size(font, 1, ".", &w1, &h1);
      y0 += h1;
      j = i + 1;
    }
  if (i == index) {
    get_utf8_info_at_index(font, len - j, s + j, index - j, cx, cy, cw, ch);
    *cy += y0;
  }
  else
    *cx = *cy = *cw = *ch = 0;
}
