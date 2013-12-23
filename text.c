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
    if (s[i] == '\n') {
      draw_utf8(dst, x, y, c, fnt, i - j, s + j);
      if (i != j)
        get_utf8_advance(fnt, i - j, s + j, &dx, &dy);
      else
        get_utf8_advance(fnt, 1, "x", &dx, &dy);
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
      if (s[i] == '\n' || i >= len) {
        if (i != j)
          get_utf8_advance(font, i - j, s + j, &w1, &h1);
        else
          get_utf8_advance(font, 1, "x", &w1, &h1);
        *w = (*w > w1) ? *w : w1;
        *h += h1;
        j = i + 1;
      }
}

int
multi_get_utf8_info_at_point(UFont font, int len, char *s, int x, int y,
                             int *cx, int *cy, int *cw, int *ch)
{
  int i, j, w, h = 0, r, yoff, asc, t;
  if (!(s && len && x >= 0 && y >= 0))
    return -1;
  for (yoff = i = j = 0; i <= len; ++i)
    if (s[i] == '\n' || i >= len) {
      yoff += h;
      if (i != j)
        get_utf8_advance(font, i - j, s + j, &w, &h);
      else
        get_utf8_advance(font, 1, "x", &w, &h);
      if (y <= h) {
        r = get_utf8_info_at_point(font, i - j, s + j, x, y, cx, cy, cw, ch);
        *cy = yoff;
        get_utf8_advance(font, 1, "x", &t, ch);
        if (r >= 0)
          return j + r;
        get_utf8_advance(font, 1, "x", cw, ch);
        *cx = w;
        return i;
      }
      y -= h;
      if (y < 0)
        return -1;
      j = i + 1;
    }
  return -1;
}

void
multi_get_utf8_info_at_index(UFont font, int len, char *s, int idx,
                             int *cx, int *cy, int *cw, int *ch)
{
  int i, j, w, h = 0, y;
  if (!(s && len && idx >= 0 && idx <= len)) {
    *cx = *cy = *cw = *ch = 0;
    return;
  }
  for (h = y = i = j = 0; i <= len; ++i)
    if (i >= len || s[i] == '\n') {
      y += h;
      if (idx > i || idx < j)
        get_utf8_advance(font, i - j, s + j, &w, &h);
      else {
        if (i != j)
          get_utf8_advance(font, i - j, s + j, &w, &h);
        else
          get_utf8_advance(font, 1, "x", &w, &h);
        break;
      }
      j = i + 1;
    }
  *cy = y;
  if (idx < len && s[idx] != '\n') {
    get_utf8_info_at_index(font, idx - j + 1, s + j, idx - j, cx, &h, cw, ch);
    get_utf8_size(font, 1, "x", &h, ch);
  } else {
    get_utf8_advance(font, idx - j, s + j, cx, &h);
    get_utf8_size(font, 1, "x", cw, ch);
  }
}
