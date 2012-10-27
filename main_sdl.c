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
int show_cursor = 1;
int frame_ms = 1000 / 30;
int moveptr_events_interval_ms = 10;
char *server_host = 0;
struct sdl_screen screen;
UFont default_font = 0;

void
draw_line(UImage dst, int x1, int y1, int x2, int y2, unsigned int c)
{
  imlib_context_set_image(dst);
  imlib_context_set_color(RGBA_R(c), RGBA_G(c), RGBA_B(c), RGBA_A(c));
  imlib_image_draw_line(x1, y1, x2, y2, 0);
}

void
draw_rect(Imlib_Image dst, int x, int y, int w, int h, unsigned int fg,
          unsigned int bg)
{
  imlib_context_set_image(dst);
  if (bg) {
    imlib_context_set_color(RGBA_R(bg), RGBA_G(bg), RGBA_B(bg), RGBA_A(bg));
    imlib_image_fill_rectangle(x, y, w, h);
  }
  if (fg) {
    imlib_context_set_color(RGBA_R(fg), RGBA_G(fg), RGBA_B(fg), RGBA_A(fg));
    imlib_image_draw_rectangle(x, y, w, h);
  }
}

void
draw_poly(UImage dst, int npts, int *pts, unsigned int fg, unsigned int bg)
{
  ImlibPolygon poly;
  int x, y;

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
free_image(UImage img)
{
  if (!img)
    return;
  imlib_context_set_image(img);
  imlib_free_image();
}

UImage
create_image(int w, int h, void *rgba)
{
  Imlib_Image img;

  img = imlib_create_image(w, h);
  if (!img)
    return 0;
  imlib_context_set_image(img);
  imlib_image_set_has_alpha(1);
  if (rgba)
    image_write_rgba(img, 0, w * h * 4, rgba);
  return img;
}

void
image_write_rgba(UImage img, unsigned int off_bytes, int len_bytes,
                 void *rgba)
{
  unsigned char *src = rgba;
  DATA32 *pixels, *dst, pix;
  int w, h, size;

  if (!img)
    return;
  imlib_context_set_image(img);
  w = imlib_image_get_width();
  h = imlib_image_get_height();
  size = w * h * 4;
  if (size <= off_bytes)
    return;
  if (off_bytes + len_bytes >= size)
    len_bytes = size - off_bytes;
  pixels = imlib_image_get_data();
  dst = pixels + (off_bytes >> 2);
  pix = *dst;
  switch (off_bytes & 3) {
  case 1:
    if (len_bytes-- > 0)
      pix = (pix & 0xffff00ff) | (*src++ << 8);
  case 2:
    if (len_bytes-- > 0)
      pix = (pix & 0xffffff00) | *src++;
  case 3:
    if (len_bytes-- > 0)
      pix = (pix & 0x00ffffff) | (*src++ << 24);
  }
  if (off_bytes & 3)
    *dst++ = pix;
  for (; len_bytes >= 4; len_bytes -= 4, src += 4, ++dst)
    *dst = src[2] | (src[1] << 8) | (src[0] << 16) | (src[3] << 24);
  if (len_bytes) {
    pix = *dst;
    switch (len_bytes) {
    case 3: pix = (pix & 0xffffff00) | src[2];
    case 2: pix = (pix & 0xffff00ff) | (src[1] << 8);
    case 1: pix = (pix & 0xff00ffff) | (src[0] << 16);
    }
    *dst = pix;
  }
  imlib_image_put_back_data(pixels);
}

void
image_read_rgba(UImage img, unsigned int off_bytes, int len_bytes, void *rgba)
{
  DATA32 *src;
  unsigned char *dst = rgba;
  int w, h, pix, size;

  if (!img)
    return;
  imlib_context_set_image(img);
  w = imlib_image_get_width();
  h = imlib_image_get_height();
  size = w * h * 4;
  if (size <= off_bytes)
    return;
  if (off_bytes + len_bytes >= size)
    len_bytes = size - off_bytes;
  src = imlib_image_get_data_for_reading_only() + (off_bytes >> 2);
  pix = *src;
  switch (off_bytes & 3) {
  case 1:
    if (len_bytes-- > 0)
      *dst++ = (pix >> 8) & 0xff;
  case 2:
    if (len_bytes-- > 0)
      *dst++ = pix & 0xff;
  case 3:
    if (len_bytes-- > 0)
      *dst++ = pix >> 24;
  }
  if (off_bytes & 3)
    pix = *(++src);
  for (; len_bytes >= 4; pix = *src++, len_bytes -= 4) {
    *dst++ = (pix >> 16) & 0xff;
    *dst++ = (pix >> 8) & 0xff;
    *dst++ = pix & 0xff;
    *dst++ = pix >> 24;
  }
  if (len_bytes)
    pix = *src;
  switch (len_bytes) {
  case 3: dst[2] = pix & 0xff;
  case 2: dst[1] = (pix >> 8) & 0xff;
  case 1: dst[0] = (pix >> 16) & 0xff;
  }
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
  imlib_blend_image_onto_image(src, 0, sx, sy, sw, sh, dx, dy, dw, dh);
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
  unsigned int prev_motion_event_ms = SDL_GetTicks();
  unsigned int prev_x[8], prev_y[8];

  while (running) {
    time_ms = SDL_GetTicks();
    while (SDL_PollEvent(&ev))
      switch (ev.type) {
      case SDL_QUIT:
        running = 0;
        break;
      case SDL_MOUSEMOTION:
        if (time_ms - prev_motion_event_ms > moveptr_events_interval_ms) {
          in_ev.type = IN_PTR_MOVE;
          in_ev.id = 0;
          in_ev.ms = time_ms;
          in_ev.x = ev.motion.x;
          in_ev.y = ev.motion.y;
          in_ev.dx = ev.motion.xrel;
          in_ev.dy = ev.motion.yrel;
          in_ev.dx = ev.motion.x - prev_x[in_ev.id];
          in_ev.dy = ev.motion.y - prev_y[in_ev.id];
          in_ev.state = ev.motion.state;
          client_input_event(&in_ev);
          prev_x[in_ev.id] = in_ev.x;
          prev_y[in_ev.id] = in_ev.y;
          prev_motion_event_ms = time_ms;
        }
        break;
      case SDL_MOUSEBUTTONUP:
      case SDL_MOUSEBUTTONDOWN:
        in_ev.type = (ev.type == SDL_MOUSEBUTTONUP) ? IN_PTR_UP : IN_PTR_DOWN;
        in_ev.id = 0;
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
  SDL_ShowCursor(show_cursor);

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
    } else if (!strcmp(argv[i], "-nocursor"))
      show_cursor = 0;
    else
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

  if (init_network())
    die("Cannot initialize network");
  fd = net_listen(server_host, server_port);
  if (fd < 0)
    die("Cannot create listening socket");
  if (sdl_init(scr_w, scr_h))
    die("Cannot init SDL");
  main_loop(fd);
  free_screen();
  free_network();
  SDL_Quit();
  return 0;
}
