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
utf8_nextchar(int i, int d, int len, char *s)
{
  for (i += d; i >= 0 && i < len && (s[i] & 0xc0) == 0x80; i += d) {}
  return i;
}

int
utf8_index_from_off(int off, int len, char *s)
{
  int j, i;
  for (i = j = 0; j < off && j < len; j = utf8_nextchar(j, 1, len, s), ++i) {}
  return i;
}

int
off_from_utf8_index(int idx, int len, char *s)
{
  int j, i;
  for (i = j = 0; i < idx && j < len; j = utf8_nextchar(j, 1, len, s), ++i) {}
  return j;
}

static int
get_utf8_info_at_point(UFont font, int len, char *s, int x, int y,
                       int *cx, int *cy, int *cw, int *ch)
{
  int j, i, w, h, tw;
  if (len < 1 || x <= 0)
    return 0;
  for (j = w = i = 0; i <= len; j = i, i = utf8_nextchar(i, 1, len, s)) {
    get_utf8_advance(font, i, s, &tw, &h);
    if (x <= tw) {
      *cx = w;
      *cy = 0;
      get_utf8_advance(font, utf8_nextchar(i, 1, len, s) - i, s + i, cw, ch);
      return j;
    }
    w = tw;
  }
  return j;
}

int
multi_get_utf8_info_at_point(UFont font, int len, char *s, int x, int y,
                             int *cx, int *cy, int *cw, int *ch)
{
  int i, j, w, h = 0, r, yoff, t;
  *cx = *cy = *cw = *ch = 0;
  if (!(s && len && x >= 0 && y >= 0))
    return -1;
  for (yoff = i = j = 0; i <= len; i = utf8_nextchar(i, 1, len, s))
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
          return utf8_index_from_off(j + r, len, s);
        get_utf8_advance(font, 1, "x", cw, ch);
        *cx = w;
        return utf8_index_from_off(i, len, s);
      }
      y -= h;
      if (y < 0)
        return -1;
      j = i + 1;
    }
  return -1;
}

static void
get_utf8_info_at_off(UFont font, int len, char *s, int off,
                     int *cx, int *cy, int *cw, int *ch)
{
  *cx = *cy = *cw = *ch = 0;
  if (off < 0 || off >= len)
    return;
  if (off > 0)
    get_utf8_advance(font, off, s, cx, cy);
  get_utf8_advance(font, utf8_nextchar(off, 1, len, s) - off, s + off,
                   cw, ch);
}

void
multi_get_utf8_info_at_index(UFont font, int len, char *s, int idx,
                             int *cx, int *cy, int *cw, int *ch)
{
  int i, j, w, h = 0, y, off;
  *cx = *cy = *cw = *ch = 0;
  off = off_from_utf8_index(idx, len, s);
  if (!(s && len && off >= 0 && off <= len))
    return;
  for (h = y = i = j = 0; i <= len; i = utf8_nextchar(i, 1, len, s))
    if (i >= len || s[i] == '\n') {
      y += h;
      if (i != j)
        get_utf8_advance(font, i - j, s + j, &w, &h);
      else
        get_utf8_advance(font, 1, "x", &w, &h);
      if (off <= i && off >= j)
        break;
      j = i + 1;
    }
  *cy = y;
  if (off < len && s[off] != '\n') {
    get_utf8_info_at_off(font, len - j, s + j, off - j, cx, &h, cw, ch);
    get_utf8_advance(font, 1, "x", &h, ch);
  } else {
    get_utf8_advance(font, off - j, s + j, cx, &h);
    get_utf8_advance(font, 1, "x", cw, ch);
  }
}

int
multi_index_vrel(UFont font, int len, char *s, int idx, int dir)
{
  int r[4], d;
  multi_get_utf8_info_at_index(font, len, s, idx, r, r + 1, r + 2, r + 3);
  d = (dir > 0) ? r[1] + r[3] + 2 : r[1] - 2;
  return multi_get_utf8_info_at_point(font, len, s, r[0] + (r[2] >> 1), d,
                                      r, r + 1, r + 2, r + 3);
}
