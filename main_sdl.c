#include <SDL/SDL.h>
#include <Imlib2.h>

#include "9p.h"
#include "net.h"
#include "util.h"
#include "input.h"
#include "draw.h"
#include "config.h"
#include "fs.h"
#include "event.h"
#include "client.h"

struct sdl_screen {
  struct screen s;
  SDL_Surface *front;
  SDL_Surface *back;
};

int server_fd = -1;
int server_port = 5558;
int scr_w = 320;
int scr_h = 200;
int frame_ms = 1000 / 30;
char *server_host = 0;
struct sdl_screen screen;
UFont default_font = 0;

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
image_get_data(UImage img, int mutable)
{
  imlib_context_set_image(img);
  if (mutable)
    return imlib_image_get_data();
  return imlib_image_get_data_for_reading_only();
}

void
image_put_back_data(UImage img, void *data)
{
  imlib_context_set_image(img);
  imlib_image_put_back_data((DATA32 *)data);
}

void
free_image(UImage img)
{
  if (!img)
    return;
  imlib_context_set_image(img);
  imlib_free_image();
}

UImage
create_image(int w, int h, void *data)
{
  Imlib_Image img;
  if (data)
    img = imlib_create_image_using_copied_data(w, h, (DATA32 *)data);
  else
    img = imlib_create_image(w, h);
  imlib_context_set_image(img);
  log_printf(LOG_UI, "image alpha: %d\n", imlib_image_has_alpha());
  imlib_image_set_has_alpha(1);
  return img;
}

UImage
resize_image(UImage img, int w, int h, int flags)
{
  UImage newimg;

  imlib_context_set_image(img);
  imlib_context_set_anti_alias(1);
  newimg = imlib_create_cropped_image(0, 0, w, h);
  if (!newimg)
    return 0;
  imlib_free_image();
  return newimg;
}

void
blit_image(UImage dst, int dx, int dy, int dw, int dh,
           UImage src, int sx, int sy, int sw, int sh)
{
  imlib_context_set_image(dst);
  imlib_context_set_anti_alias(1);
  imlib_context_set_blend(1);
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
    default_font = create_font(DEFAULT_FONT, DEFAULT_FONT_SIZE, "");
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
  memset(screen.s.pixels, 0xff, size);
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
      = imlib_create_image_using_data(w, h, (DATA32 *)screen.s.pixels);
  if (!screen.s.blit) {
    SDL_FreeSurface(screen.back);
    free(screen.s.pixels);
  }
  imlib_context_set_image(screen.s.blit);
  imlib_image_set_has_alpha(0);
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
draw_utf8(UImage dst, int x, int y, int c, UFont font, int len, char *str)
{
  char let;

  if (!(len && str))
    return;

  imlib_context_set_font((font) ? font : default_font);
  imlib_context_set_image(dst);
  imlib_context_set_color(RGBA_R(c), RGBA_G(c), RGBA_B(c), RGBA_A(c));

  /* FIXME: use patched imlib2 */
  let = str[len];
  str[len] = 0;
  imlib_text_draw(x, y, str);
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
  str[len] = 0;
  imlib_get_text_advance(str, w, h);
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
main_loop(int server_fd)
{
  SDL_Event ev;
  struct input_event in_ev;
  int running = 1;
  unsigned int prev_draw_ms = 0, time_ms;

  while (running) {
    time_ms = SDL_GetTicks();
    while (SDL_PollEvent(&ev)) {
      /*log_printf(LOG_DBG, "#SDL ev.type: %d\n", ev.type);*/
      switch (ev.type) {
      case SDL_QUIT:
        running = 0;
        break;
      case SDL_MOUSEMOTION:
        in_ev.type = IN_PTR_MOVE;
        in_ev.ms = time_ms;
        in_ev.x = ev.motion.x;
        in_ev.y = ev.motion.y;
        in_ev.dx = ev.motion.xrel;
        in_ev.dy = ev.motion.yrel;
        in_ev.state = ev.motion.state;
        client_input_event(&in_ev);
        break;
      case SDL_MOUSEBUTTONUP:
      case SDL_MOUSEBUTTONDOWN:
        in_ev.type = (ev.type == SDL_MOUSEBUTTONUP) ? IN_PTR_UP : IN_PTR_DOWN;
        in_ev.ms = time_ms;
        in_ev.x = ev.button.x;
        in_ev.y = ev.button.y;
        in_ev.key = ev.button.button;
        client_input_event(&in_ev);
        break;
      case SDL_KEYUP:
        if (ev.key.keysym.sym == SDLK_ESCAPE
            && (ev.key.keysym.mod & (KMOD_LCTRL | KMOD_RCTRL)))
          running = 0;
      case SDL_KEYDOWN:
        in_ev.type = (ev.type == SDL_KEYUP) ? IN_KEY_UP : IN_KEY_DOWN;
        in_ev.ms = time_ms;
        in_ev.key = ev.key.keysym.sym;
        in_ev.state = ev.key.keysym.mod;
        in_ev.unicode = ev.key.keysym.unicode;
        client_input_event(&in_ev);
        break;
      }
    }
    if (process_clients(server_fd, time_ms, frame_ms))
      running = 0;
    if (time_ms - prev_draw_ms > frame_ms) {
      if (draw_clients())
        refresh_screen();
      prev_draw_ms = time_ms;
    }
  }
  return 0;
}

int
sdl_init(int w, int h)
{
  if (SDL_Init(SDL_INIT_VIDEO) < 0)
    return -1;

  if (init_screen(w, h))
    return -1;
  return 0;
}

void
parse_args(int argc, char **argv)
{
  int i, j;

  for (i = 1; i < argc; ++i)
    if (!strcmp(argv[i], "-d") && i + 1 < argc)
      for (++i, j = 0; argv[i][j]; ++j)
        switch (argv[i][j]) {
        case 'c': logmask |= LOG_CLIENT; break;
        case 'd': logmask |= LOG_DATA; break;
        case 'g': logmask |= LOG_DBG; break;
        case 'm': logmask |= LOG_MSG; break;
        case 'u': logmask |= LOG_UI; break;
        }
    else if (!strcmp(argv[i], "-s") && i + 1 < argc) {
      if (sscanf(argv[++i], "%dx%d", &scr_w, &scr_h) != 2 || scr_w <= 0
          || scr_h <= 0)
        die("Wrong resolution.");
    } else
      die("Usage: uifs [-d logmask] [-s WxH]");
  log_printf(LOG_CLIENT, "logging: client\n");
  log_printf(LOG_DATA, "logging: data\n");
  log_printf(LOG_DBG, "logging: dbg\n");
  log_printf(LOG_MSG, "logging: msg\n");
  log_printf(LOG_UI, "logging: ui\n");
}

int
main(int argc, char **argv)
{
  int fd;

  parse_args(argc, argv);

  fd = net_listen(server_host, server_port);
  if (fd < 0)
    die("Cannot create listening socket");
  if (sdl_init(scr_w, scr_h))
    die("Cannot init SDL");
  main_loop(fd);
  free_screen();
  SDL_Quit();
  return 0;
}
