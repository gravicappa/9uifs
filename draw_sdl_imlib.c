#include <Imlib2.h>
#include <SDL/SDL.h>

#include "util.h"
#include "draw.h"
#include "config.h"

struct sdl_screen {
  struct screen s;
  SDL_Surface *front;
  SDL_Surface *back;
};

struct sdl_screen screen;
Font default_font = 0;

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

void *
image_get_data(Image img, int mutable)
{
  imlib_context_set_image(img);
  if (mutable)
    return imlib_image_get_data();
  return imlib_image_get_data_for_reading_only();
}

void
image_put_back_data(Image img, void *data)
{
  imlib_context_set_image(img);
  imlib_image_put_back_data((DATA32 *)data);
}

void
free_image(Image img)
{
  imlib_context_set_image(img);
  imlib_free_image();
}

Image
create_image(int w, int h)
{
  return imlib_create_image(w, h);
}

Image
resize_image(Image img, int w, int h, int flags)
{
  Image newimg;

  imlib_context_set_image(img);
  imlib_context_set_anti_alias(1);
  newimg = imlib_create_cropped_image(0, 0, w, h);
  if (!newimg)
    return 0;
  imlib_free_image();
  return newimg;
}

void
blit_image(Image dst, int dx, int dy, int dw, int dh,
           Image src, int sx, int sy, int sw, int sh)
{
  imlib_context_set_image(dst);
  imlib_context_set_anti_alias(1);
  imlib_context_set_blend(0);
  imlib_blend_image_onto_image(src, 0, sx, dy, sw, sh, dx, dy, dw, dh);
}

void
set_cliprect(int x, int y, int w, int h)
{
  imlib_context_set_cliprect(x, y, w, h);
}

struct screen *
default_screen()
{
  return &screen.s;
}

static void
init_fonts()
{
  imlib_add_path_to_font_path(DEFAULT_FONT_DIR);
  if (!default_font)
    default_font = create_font(DEFAULT_FONT, DEFAULT_FONT_SIZE);
}

int
init_screen(int w, int h)
{
  int size;

  screen.s.w = w;
  screen.s.h = h;
  screen.front = SDL_SetVideoMode(w, h, 0, SDL_HWSURFACE | SDL_ANYFORMAT);
  if (!screen.front)
    return -1;
  size = screen.front->w * screen.front->h * 4;
  screen.s.pixels = (char *)malloc(size);
  if (!screen.s.pixels)
    return -1;
  memset(screen.s.pixels, 0, size);
  screen.back = SDL_CreateRGBSurfaceFrom(screen.s.pixels,
                                         screen.s.w, screen.s.h, 32,
                                         screen.s.w * 4,
                                         0x00ff0000, 0x0000ff00, 0x000000ff,
                                         0xff000000);
  if (!screen.back) {
    free(screen.s.pixels);
    return -1;
  }
  screen.s.blit
      = imlib_create_image_using_data(w, h, (DATA32 *) screen.s.pixels);
  if (!screen.s.blit) {
    SDL_FreeSurface(screen.back);
    free(screen.s.pixels);
  }
  init_fonts();
  return 0;
}

void
free_screen()
{
  if (screen.s.blit) {
    free_image(screen.s.blit);
    screen.s.blit = 0;
  }
  if (screen.back) {
    SDL_FreeSurface(screen.back);
    screen.back = 0;
  }
  if (screen.s.pixels) {
    free(screen.s.pixels);
    screen.s.pixels = 0;
  }
  if (default_font) {
    free_font(default_font);
    default_font = 0;
  }
}

void
refresh_screen()
{
  SDL_BlitSurface(screen.back, 0, screen.front, 0);
  SDL_UpdateRect(screen.front, 0, 0, 0, 0);
}

void
draw_utf8(Image dst, int x, int y, int c, Font font, char *str)
{
  imlib_context_set_font((font) ? font : default_font);
  imlib_context_set_image(dst);
  imlib_context_set_color(RGBA_R(c), RGBA_G(c), RGBA_B(c), RGBA_A(c));
  imlib_text_draw(x, y, str);
}

int
get_utf8_size(Font font, char *str, int *w, int *h)
{
  imlib_context_set_font((font) ? font : default_font);
  imlib_get_text_advance(str, w, h);
  return 0;
}

Font
create_font(const char *name, int size)
{
  char buf[256];

  snprintf(buf, sizeof(buf), "%s/%d", name, size);
  return imlib_load_font(buf);
}

void
free_font(Font font)
{
  if (font) {
    imlib_context_set_font(font);
    imlib_free_font();
  }
}
