#include <SDL/SDL.h>
#include <SDL/SDL_thread.h>
#include <Imlib2.h>
#include <unistd.h>

#include "net.h"
#include "util.h"
#include "api.h"
#include "raster.h"
#include "backend.h"
#include "dirty.h"
#include "config.h"
#include "profile.h"

static int sdl_flags = SDL_HWSURFACE | SDL_DOUBLEBUF | SDL_ANYFORMAT;
static int scr_w = 320;
static int scr_h = 200;
static int show_cursor = 1;
static int frame_ms = 1000 / 30;
static int moveptr_events_interval_ms = 1000 / 30;
static char *server_host = 0;
static UFont default_font = 0;

static SDL_mutex *event_mutex;
static SDL_cond *event_cond;
static SDL_Event event;
static int event_pipe[2] = {-1, -1};
static SDL_Surface *screen = 0;
static SDL_Surface *backbuffer = 0;

Imlib_Image screen_image = 0;

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

static void
init_fonts(void)
{
  imlib_add_path_to_font_path(DEF_FONT_DIR);
  if (!default_font)
    default_font = create_font(DEF_FONT, DEF_FONT_SIZE, "");
}

static int
init_screen(int w, int h)
{
  SDL_PixelFormat *fmt;

  screen = SDL_SetVideoMode(w, h, 0, sdl_flags);
  if (!screen) {
    log_printf(LOG_ERR, "Cannot change video mode.\n");
    return -1;
  }
  fmt = screen->format;
  if (!(fmt->BitsPerPixel == 32 && fmt->Rmask == 0x00ff0000
        && fmt->Gmask == 0x0000ff00 && fmt->Bmask == 0x000000ff
        && fmt->Amask == 0xff000000)) {
    backbuffer = SDL_CreateRGBSurface(0, screen->w, screen->h, 32,
                                      0x00ff0000, 0x0000ff00, 0x000000ff,
                                      0xff000000);
    if (!backbuffer)
      return -1;
  }
  init_fonts();
  init_dirty(0, 0, w, h);
  return 0;
}

static void
free_screen(void)
{
  if (backbuffer) {
    SDL_FreeSurface(backbuffer);
    backbuffer = 0;
  }
  if (default_font) {
    free_font(default_font);
    default_font = 0;
  }
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

unsigned int
current_time_ms(void)
{
  return SDL_GetTicks();
}

static const char *
event_type_str(SDL_Event *ev)
{
#define CASE(x) case x: return #x
  switch (ev->type) {
  CASE(SDL_QUIT);
  CASE(SDL_VIDEOEXPOSE);
  CASE(SDL_VIDEORESIZE);
  CASE(SDL_MOUSEMOTION);
  CASE(SDL_MOUSEBUTTONUP);
  CASE(SDL_MOUSEBUTTONDOWN);
  CASE(SDL_KEYUP);
  CASE(SDL_KEYDOWN);
  CASE(SDL_USEREVENT);
  default: return "unknown";
  }
#undef CASE
}

static int
event_thread_fn(void *aux)
{
  int thread_running;
  SDL_Event ev;

  for (thread_running = 1; thread_running;) {
    event.type = SDL_NOEVENT;
    if (SDL_WaitEvent(&ev) == 1) {
      if (SDL_mutexP(event_mutex) < 0) {
        log_printf(LOG_DBG, "event thread: cannot lock mutex\n");
        return -1;
      }
      memcpy(&event, &ev, sizeof(event));
      write(event_pipe[1], "x", 1);
      switch (ev.type) {
        case SDL_USEREVENT:
        case SDL_QUIT:
          thread_running = 0;
          break;
        default:
          SDL_CondWait(event_cond, event_mutex);
      }
      if (SDL_mutexV(event_mutex) < 0)
        return -1;
    }
  }
  return 0;
}

static int
process_event(SDL_Event *ev, unsigned int time_ms, int *redraw_all)
{
  struct input_event in_ev = {0};

  switch (ev->type) {
  case SDL_QUIT:
    return -1;
  case SDL_VIDEOEXPOSE:
  case SDL_VIDEORESIZE:
    *redraw_all = 1;
    break;
  case SDL_MOUSEMOTION:
    in_ev.type = IN_PTR_MOVE;
    in_ev.id = 0;
    in_ev.ms = time_ms;
    in_ev.x = ev->motion.x;
    in_ev.y = ev->motion.y;
    in_ev.dx = ev->motion.xrel;
    in_ev.dy = ev->motion.yrel;
    in_ev.state = ev->motion.state;
    uifs_input_event(&in_ev);
    break;
  case SDL_MOUSEBUTTONUP:
  case SDL_MOUSEBUTTONDOWN:
    in_ev.type = (ev->type == SDL_MOUSEBUTTONUP) ? IN_PTR_UP : IN_PTR_DOWN;
    in_ev.id = 0;
    in_ev.ms = time_ms;
    in_ev.x = ev->button.x;
    in_ev.y = ev->button.y;
    in_ev.key = ev->button.button;
    uifs_input_event(&in_ev);
    break;
  case SDL_KEYUP:
    if (ev->key.keysym.sym == SDLK_ESCAPE
        && (ev->key.keysym.mod & (KMOD_LCTRL | KMOD_RCTRL)))
      return -1;
  case SDL_KEYDOWN:
    in_ev.type = (ev->type == SDL_KEYUP) ? IN_KEY_UP : IN_KEY_DOWN;
    in_ev.ms = time_ms;
    in_ev.key = ev->key.keysym.sym;
    in_ev.state = ev->key.keysym.mod;
    in_ev.unicode = ev->key.keysym.unicode;
    uifs_input_event(&in_ev);
    break;
  default:;
  }
  return 0;
}

static int
redraw(int force_redraw)
{
  SDL_Surface *s = (backbuffer) ? backbuffer : screen;
  int i, *rect;

  if (SDL_MUSTLOCK(s) && SDL_LockSurface(s) < 0)
    return -1;
  screen_image = imlib_create_image_using_data(s->w, s->h, s->pixels);
  if (uifs_redraw(force_redraw)) {
    if (screen->flags & SDL_DOUBLEBUF)
      SDL_Flip(screen);
    else for (i = 0, rect = dirty_rects; i < ndirty_rects; ++i, rect += 4)
      SDL_UpdateRect(screen, rect[0], rect[1], rect[2], rect[3]);
  }
  free_image(screen_image);
  if (SDL_MUSTLOCK(s))
    SDL_UnlockSurface(s);
  return 0;
}

static int
main_loop(int server_fd)
{
  SDL_Thread *event_thread;
  SDL_Event quit_event = {0};
  unsigned int prev_draw_ms = 0, time_ms;
  int running, force_redraw = 1;

  if (pipe(event_pipe) < 0)
    die("Cannot create event pipe");

  event_cond = SDL_CreateCond();
  if (!event_cond)
    die("Cannot create condition variable");

  event_mutex = SDL_CreateMutex();
  if (!event_mutex)
    die("Cannot create event mutex");

  event_thread = SDL_CreateThread(event_thread_fn, 0);
  if (!event_thread)
    die("Cannot create event thread");

  for (running = 1; running;) {
    profile_start(PROF_LOOP);
    time_ms = SDL_GetTicks();
    profile_start(PROF_IO);
    if (uifs_process_io(server_fd, event_pipe[0], frame_ms))
      running = 0;
    profile_start(PROF_EVENTS);
    if (SDL_mutexP(event_mutex) < 0)
      die("unable to lock mutex");
    if (event.type != SDL_NOEVENT) {
      if (process_event(&event, time_ms, &force_redraw) < 0)
        running = 0;
      SDL_PumpEvents();
      while (SDL_PollEvent(&event))
        if (process_event(&event, time_ms, &force_redraw) < 0)
          running = 0;
      event.type = SDL_NOEVENT;
      SDL_CondSignal(event_cond);
    }
    if (SDL_mutexV(event_mutex) < 0)
      die("unable to unlock mutex");
    profile_end(PROF_EVENTS);
    profile_end(PROF_IO);
    profile_start(PROF_DRAW);
    if ((force_redraw || uifs_update())
        && (time_ms - prev_draw_ms > frame_ms)) {
      redraw(force_redraw);
      force_redraw = 0;
      prev_draw_ms = time_ms;
    }
    profile_end(PROF_DRAW);
    profile_end(PROF_LOOP);
  }
  quit_event.type = SDL_QUIT;
  if (SDL_PushEvent(&quit_event) < 0)
    log_printf(LOG_ERR, "push quit event failed\n");
  SDL_WaitThread(event_thread, 0);
  SDL_DestroyCond(event_cond);
  SDL_DestroyMutex(event_mutex);
  profile_show();
  return 0;
}

static int
sdl_init(int w, int h)
{
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    log_printf(LOG_ERR, "SDL_Init failed.\n");
    return -1;
  }
  SDL_ShowCursor(show_cursor);

  if (init_screen(w, h))
    return -1;
  return 0;
}

static void
parse_args(int argc, char **argv)
{
  int i, j;

  for (i = 1; i < argc; ++i)
    if (!strcmp(argv[i], "-d") && i + 1 < argc)
      for (++i, j = 0; argv[i][j]; ++j)
        switch (argv[i][j]) {
        case 'e': logmask |= LOG_ERR; break;
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
    } else if (!strcmp(argv[i], "-f") && i + 1 < argc) {
      if (sscanf(argv[++i], "%d", &j) != 1 || j <= 0)
        die("Wrong frames per second.");
      moveptr_events_interval_ms = frame_ms = 1000 / j;
    } else if (!strcmp(argv[i], "-nocursor"))
      show_cursor = 0;
    else
      die("Usage: uifs [-d logmask] [-s WxH] [-nocursor]");
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
    die("Cannot initialize network.");
  fd = net_listen(server_host, server_port);
  if (fd < 0)
    die("Cannot create listening socket.");
  if (sdl_init(scr_w, scr_h))
    die("Cannot init SDL.");
  main_loop(fd);
  close(fd);
  free_screen();
  free_network();
  SDL_Quit();
  return 0;
}
