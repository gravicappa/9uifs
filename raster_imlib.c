#include <Imlib2.h>
#include <stdio.h>
#include "frontend.h"
#include "raster.h"
#include "dirty.h"

UFont default_font = 0;

void
draw_line(Imlib_Image dst, int x1, int y1, int x2, int y2, unsigned int c)
{
  if (dst && (c & 0xff000000)) {
    imlib_context_set_image(dst);
    imlib_context_set_color(RGBA_R(c), RGBA_G(c), RGBA_B(c), RGBA_A(c));
    imlib_image_draw_line(x1, y1, x2, y2, 0);
  }
}

void
draw_rect(Imlib_Image dst, int x, int y, int w, int h, unsigned int fg,
          unsigned int bg)
{
  if (!dst)
    return;
  imlib_context_set_image(dst);
  if (bg & 0xff000000) {
    imlib_context_set_color(RGBA_R(bg), RGBA_G(bg), RGBA_B(bg), RGBA_A(bg));
    imlib_image_fill_rectangle(x, y, w, h);
  }
  if (fg & 0xff000000) {
    imlib_context_set_color(RGBA_R(fg), RGBA_G(fg), RGBA_B(fg), RGBA_A(fg));
    imlib_image_draw_rectangle(x, y, w, h);
  }
}

void
draw_poly(Imlib_Image dst, int npts, int *pts, unsigned int fg,
          unsigned int bg)
{
  ImlibPolygon poly;

  if (!dst)
    return;
  poly = imlib_polygon_new();
  if (!poly)
    return;
  for (; npts; npts--, pts += 2)
    imlib_polygon_add_point(poly, pts[0], pts[1]);
  imlib_context_set_image(dst);
  if (bg) {
    imlib_context_set_color(RGBA_R(bg), RGBA_G(bg), RGBA_B(bg), RGBA_A(bg));
    imlib_image_fill_polygon(poly);
  }
  if (fg) {
    imlib_context_set_color(RGBA_R(fg), RGBA_G(fg), RGBA_B(fg), RGBA_A(fg));
    imlib_image_draw_polygon(poly, (bg) ? 1 : 0);
  }
  imlib_polygon_free(poly);
}

void
free_image(Imlib_Image img)
{
  if (!img)
    return;
  imlib_context_set_image(img);
  imlib_free_image();
}

Imlib_Image
create_image(int w, int h, void *rgba)
{
  struct image *img;

  img = imlib_create_image(w, h);
  if (!img)
    return 0;
  imlib_context_set_image(img);
  imlib_image_set_has_alpha(1);
  if (rgba)
    image_write_rgba(img, 0, w * h * 4, rgba);
  return img;
}

int
image_get_size(Imlib_Image img, int *w, int *h)
{
  if (!img) {
    *w = *h = 0;
    return -1;
  }
  imlib_context_set_image(img);
  *w = imlib_image_get_width();
  *h = imlib_image_get_height();
  return 0;
}

void
image_write_rgba(Imlib_Image img, unsigned int off_bytes, int len_bytes,
                 void *rgba)
{
  int w, h;
  DATA32 *pixels;

  if (!img)
    return;
  imlib_context_set_image(img);
  w = imlib_image_get_width();
  h = imlib_image_get_height();
  pixels = imlib_image_get_data();
  rgba_pixels_to_argb_image(off_bytes, len_bytes, w * h * 4, rgba, pixels);
  imlib_image_put_back_data(pixels);
}

void
image_read_rgba(UImage img, unsigned int off_bytes, int len_bytes, void *rgba)
{
  int w, h;

  if (!img)
    return;
  imlib_context_set_image(img);
  w = imlib_image_get_width();
  h = imlib_image_get_height();
  rgba_pixels_from_argb_image(off_bytes, len_bytes, w * h * 4,
                              rgba, imlib_image_get_data_for_reading_only());
}

UImage
resize_image(UImage img, int w, int h, int flags)
{
  UImage newimg;

  if (!img);
    return 0;

  imlib_context_set_image(img);
  imlib_context_set_anti_alias(1);
  newimg = imlib_create_cropped_image(0, 0, w, h);
  if (!newimg)
    return 0;
  imlib_free_image();
  return img;
}

void
blit_image(UImage dst, int dx, int dy, int dw, int dh,
           UImage src, int sx, int sy, int sw, int sh)
{
  if (!(dst && src))
    return;
  dw = (dw < 0) ? sw : dw;
  dh = (dh < 0) ? sh : dh;
  imlib_context_set_image(dst);
  imlib_context_set_anti_alias(1);
  imlib_context_set_blend(1);
  imlib_blend_image_onto_image(src, 0, sx, sy, sw, sh, dx, dy, dw, dh);
}

void
set_cliprect(int x, int y, int w, int h)
{
  imlib_context_set_cliprect(x, y, w, h);
}

void
draw_utf8(UImage dst, int x, int y, int c, UFont font, int len, char *str)
{
  char let;

  if (!(len && str && dst))
    return;

  imlib_context_set_font((font) ? font : default_font);
  imlib_context_set_image(dst);
  imlib_context_set_color(RGBA_R(c), RGBA_G(c), RGBA_B(c), RGBA_A(c));

  let = str[len];
  if (let)
    str[len] = 0;
  imlib_text_draw(x, y, str);
  if (let)
    str[len] = let;
}

int
get_utf8_size(UFont font, int len, char *str, int *w, int *h)
{
  char let;

  if (!(len && str)) {
    *w = *h = 0;
    return 0;
  }
  imlib_context_set_font((font) ? font : default_font);

  /* FIXME: use patched imlib2 */
  let = str[len];
  if (let)
    str[len] = 0;
  imlib_get_text_advance(str, w, h);
  if (let)
    str[len] = let;
  return 0;
}

UFont
create_font(const char *name, int size, const char *style)
{
  char buf[256];

  snprintf(buf, sizeof(buf), "%s/%d", name, size);
  return imlib_load_font(buf);
}

void
free_font(UFont font)
{
  if (font) {
    imlib_context_set_font(font);
    imlib_free_font();
  }
}

const char **
font_list(int *n)
{
  return (const char **)imlib_list_fonts(n);
}

int
get_utf8_info_at_point(UFont font, int len, char *str, int x, int y,
                       int *cx, int *cy, int *cw, int *ch)
{
  int let, ret;

  imlib_context_set_font((font) ? font : default_font);
  let = str[len];
  if (let)
    str[len] = 0;
  ret = imlib_text_get_index_and_location(str, x, y, cx, cy, cw, ch);
  if (let)
    str[len] = let;
  return ret;
}

void
get_utf8_info_at_index(UFont font, int len, char *str, int index,
                       int *cx, int *cy, int *cw, int *ch)
{
  int let;

  imlib_context_set_font((font) ? font : default_font);
  let = str[len];
  if (let)
    str[len] = 0;
  imlib_text_get_location_at_index(str, index, cx, cy, cw, ch);
  if (let)
    str[len] = let;
}
