#include <Imlib2.h>

#include "draw.h"

void
fill_rect(Imlib_Image dst, int x, int y, int w, int h, unsigned int c)
{
  imlib_context_set_image(dst);
  imlib_context_set_color(RGBA_R(c), RGBA_G(c), RGBA_B(c), RGBA_A(c));
  imlib_image_fill_rectangle(x, y, w, h);
}

void
draw_rect(Imlib_Image dst, int x, int y, int w, int h, unsigned int c)
{
  imlib_context_set_image(dst);
  imlib_context_set_color(RGBA_R(c), RGBA_G(c), RGBA_B(c), RGBA_A(c));
  imlib_image_draw_rectangle(x, y, w, h);
}
