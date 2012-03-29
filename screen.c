#include <SDL/SDL.h>
#include <Imlib2.h>

#include "util.h"
#include "screen.h"

struct screen screen;

int
init_screen(int w, int h)
{
  int size;

  screen.front = SDL_SetVideoMode(w, h, 0, SDL_HWSURFACE | SDL_ANYFORMAT);
  if (!screen.front)
    return -1;
  size = screen.front->w * screen.front->h * 4;
  screen.pixels = (char *)malloc(size);
  if (!screen.pixels)
    return -1;
  memset(screen.pixels, 0, size);
  screen.back = SDL_CreateRGBSurfaceFrom(screen.pixels,
                                      screen.front->w, screen.front->h, 32,
                                      screen.front->w * 4,
                                      0x00ff0000, 0x0000ff00, 0x000000ff,
                                      0xff000000);
  if (!screen.back) {
    free(screen.pixels);
    return -1;
  }
  screen.imlib = imlib_create_image_using_data(w, h, (DATA32 *)screen.pixels);
  if (!screen.imlib) {
    SDL_FreeSurface(screen.back);
    free(screen.pixels);
  }
  return 0;
}

void
release_screen()
{
  if (screen.imlib) {
    imlib_context_set_image(screen.imlib);
    imlib_free_image();
    screen.imlib = 0;
  }
  if (screen.back) {
    SDL_FreeSurface(screen.back);
    screen.back = 0;
  }
  if (screen.pixels) {
    free(screen.pixels);
    screen.pixels = 0;
  }
}

void
refresh_screen()
{
  SDL_BlitSurface(screen.back, 0, screen.front, 0);
  SDL_UpdateRect(screen.front, 0, 0, 0, 0);
}
