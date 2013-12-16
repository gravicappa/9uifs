#include <SDL/SDL.h>
#include <SDL/SDL_thread.h>
#include <Imlib2.h>
#include <unistd.h>
#include <stdlib.h>

#include "net.h"
#include "util.h"
#include "api.h"
#include "frontend.h"
#include "dirty.h"
#include "config.h"
#include "profile.h"

static int sdl_flags = SDL_HWSURFACE | SDL_DOUBLEBUF | SDL_ANYFORMAT;
static int show_cursor = 1;
static int frame_ms = 1000 / 30;
static int moveptr_events_interval_ms = 1000 / 30;
static char *server_host = 0;
extern UFont default_font;

static SDL_mutex *event_mutex;
static SDL_cond *event_cond;
static SDL_Event event;
static int event_pipe[2] = {-1, -1};
static SDL_Surface *screen = 0;
static SDL_Surface *backbuffer = 0;

int screen_w = 320;
int screen_h = 200;
Imlib_Image screen_image = 0;

enum flags {
  REDRAW = 1,
  FORCE_REDRAW = 2,
  FORCE_UPDATE = 4 | FORCE_REDRAW,
};

unsigned int
current_time_ms(void)
{
  return SDL_GetTicks();
}

static void
init_fonts(void)
{
  imlib_add_path_to_font_path(DEF_FONT_DIR);
  if (!default_font)
    default_font = create_font(DEF_FONT, DEF_FONT_SIZE, "");
}

static void
show_info(const SDL_VideoInfo *info)
{
  char buffer[256];

  log_printf(LOG_FRONT, ";; Info: %p\n", info);
  if (SDL_VideoDriverName(buffer, sizeof(buffer))) {
    log_printf(LOG_FRONT, ";; Video driver: %s\n", buffer);
  }
  if (info) {
    log_printf(LOG_FRONT, ";; Video info:\n");
    log_printf(LOG_FRONT, ";;       hw: %d\n", info->hw_available);
    log_printf(LOG_FRONT, ";;       blit_hw: %d\n", info->blit_hw);
    log_printf(LOG_FRONT, ";;       blit_hw_CC: %d\n", info->blit_hw_CC);
    log_printf(LOG_FRONT, ";;       blit_hw_A: %d\n", info->blit_hw_A);
    log_printf(LOG_FRONT, ";;       blit_sw: %d\n", info->blit_sw);
    log_printf(LOG_FRONT, ";;       blit_sw_CC: %d\n", info->blit_sw_CC);
    log_printf(LOG_FRONT, ";;       blit_sw_A: %d\n", info->blit_sw_A);
    log_printf(LOG_FRONT, ";;       blit_fill: %d\n", info->blit_fill);
    log_printf(LOG_FRONT, ";;       mem: %d\n", info->video_mem);
    if (info->vfmt) {
      log_printf(LOG_FRONT, ";;      depth: %d bytes/pixel\n",
                 info->vfmt->BytesPerPixel);
      log_printf(LOG_FRONT, ";;      rmask: %08x\n", info->vfmt->Rmask);
      log_printf(LOG_FRONT, ";;      gmask: %08x\n", info->vfmt->Gmask);
      log_printf(LOG_FRONT, ";;      bmask: %08x\n", info->vfmt->Bmask);
      log_printf(LOG_FRONT, ";;      amask: %08x\n", info->vfmt->Amask);
    }
  }
}

static const char *
str_from_flags(unsigned int flags)
{
  static char buf[1024];
#define IS(x) ((flags & x) == x)
  snprintf(buf, sizeof(buf), "[dbuf: %d hw: %d glblit: %d hwacc:%d]",
           IS(SDL_DOUBLEBUF), IS(SDL_HWSURFACE), IS(SDL_OPENGLBLIT),
           IS(SDL_HWACCEL));
  return buf;
#undef IS
}

static void
show_surface_info(SDL_Surface *s, const char *name)
{
  if (s) {
    log_printf(LOG_FRONT, ";; surface '%s'\n", name);
    log_printf(LOG_FRONT, ";;         w: %d\n", s->w);
    log_printf(LOG_FRONT, ";;         h: %d\n", s->h);
    log_printf(LOG_FRONT, ";;         pitch: %d\n", s->pitch);
    log_printf(LOG_FRONT, ";;         flags: %s\n", str_from_flags(s->flags));
    if (s->format) {
      log_printf(LOG_FRONT, ";;         depth: %d bytes/pixel\n",
                 s->format->BytesPerPixel);
      log_printf(LOG_FRONT, ";;         depth: %d bits/pixel\n",
                 s->format->BitsPerPixel);
      log_printf(LOG_FRONT, ";;         rmask: %08x\n", s->format->Rmask);
      log_printf(LOG_FRONT, ";;         gmask: %08x\n", s->format->Gmask);
      log_printf(LOG_FRONT, ";;         bmask: %08x\n", s->format->Bmask);
      log_printf(LOG_FRONT, ";;         amask: %08x\n", s->format->Amask);
    }
  }
}

static int
imlib_compatible_fmt(SDL_PixelFormat *fmt)
{
  return fmt && (fmt->BitsPerPixel == 32 && fmt->Rmask == 0x00ff0000
                 && fmt->Gmask == 0x0000ff00 && fmt->Bmask == 0x000000ff);
}

static int
init_screen()
{
  const SDL_VideoInfo *info;

  info = SDL_GetVideoInfo();
  screen_w = (screen_w > 0) ? screen_w : info->current_w;
  screen_h = (screen_h > 0) ? screen_h : info->current_h;
  show_info(info);
  log_printf(LOG_FRONT,
             "sdl-mode res: %dx%d flags: %08x (%s) compatible: %d\n",
             screen_w, screen_h, sdl_flags, str_from_flags(sdl_flags),
             imlib_compatible_fmt(info->vfmt));
  if (!imlib_compatible_fmt(info->vfmt))
    sdl_flags &= ~SDL_DOUBLEBUF;
  log_printf(LOG_FRONT, "  flags: %s\n", str_from_flags(sdl_flags));
  screen = SDL_SetVideoMode(screen_w, screen_h, 0, sdl_flags);
  show_surface_info(screen, "screen");
  if (!screen) {
    log_printf(LOG_ERR, "Cannot change video mode.\n");
    return -1;
  }
  if (!imlib_compatible_fmt(screen->format)) {
    backbuffer = SDL_CreateRGBSurface(0, screen->w, screen->h, 32,
                                      0x00ff0000, 0x0000ff00, 0x000000ff,
                                      0x00000000);
    if (!backbuffer)
      return -1;
    SDL_SetAlpha(backbuffer, 0, 0xff);
    SDL_FillRect(backbuffer, 0, 0xffffffff);
  }
  init_fonts();
  init_dirty(0, 0, screen_w, screen_h);
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
        log_printf(LOG_ERR, "event thread: cannot lock mutex\n");
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
process_event(SDL_Event *ev, unsigned int time_ms, int *flags)
{
  struct input_event in_ev = {0};

  switch (ev->type) {
  case SDL_QUIT:
    return -1;
  case SDL_VIDEOEXPOSE:
    *flags |= FORCE_REDRAW;
  case SDL_VIDEORESIZE:
    *flags |= FORCE_UPDATE;
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
    in_ev.mod = ev->key.keysym.mod;
    in_ev.unicode = ev->key.keysym.unicode;
    uifs_input_event(&in_ev);
    break;
  default:;
  }
  return 0;
}

static void
draw_grid(Imlib_Image dst)
{
  int i, di, dj, w, h;
  di = dj = 8;
  image_get_size(dst, &w, &h);
  for (i = 0; i < w; i += di)
    draw_line(dst, i, 0, i, h, 0x7f7f7f7f);
  for (i = 0; i < h; i += dj)
    draw_line(dst, 0, i, w, i, 0x7f7f7f7f);
}

static void
draw_dirty_rects(Imlib_Image dst)
{
  int *r, i;
  for (i = 0, r = dirty_rects; i < ndirty_rects; ++i, r += 4)
    draw_rect(dst, r[0], r[1], r[2], r[3], 0x80ff0000, 0);
}

static int
draw(int redraw_all)
{
  SDL_Surface *s = (backbuffer) ? backbuffer : screen;
  SDL_Rect blitrect;
  int i, *rect, redrawn;

  profile_start(PROF_DRAW);
  if (SDL_MUSTLOCK(s) && SDL_LockSurface(s) < 0)
    return -1;
  screen_image = imlib_create_image_using_data(s->w, s->h, s->pixels);
  if (0) draw_grid(screen_image);
  redrawn = uifs_redraw(redraw_all);
  if (0 && redrawn)
    draw_dirty_rects(screen_image);
  free_image(screen_image);
  screen_image = 0;
  if (SDL_MUSTLOCK(s))
    SDL_UnlockSurface(s);
  profile_end(PROF_DRAW);
  if (redrawn) {
    profile_start(PROF_DRAW_BLIT);
    if (backbuffer)
      for (i = 0, rect = dirty_rects; i < ndirty_rects; ++i, rect += 4) {
        blitrect.x = rect[0];
        blitrect.y = rect[1];
        blitrect.w = rect[2];
        blitrect.h = rect[3];
        SDL_BlitSurface(backbuffer, &blitrect, screen, &blitrect);
      }
    if (sdl_flags & SDL_DOUBLEBUF)
      SDL_Flip(screen);
    else for (i = 0, rect = dirty_rects; i < ndirty_rects; ++i, rect += 4)
      SDL_UpdateRect(screen, rect[0], rect[1], rect[2], rect[3]);
    profile_end(PROF_DRAW_BLIT);
  }
  return 0;
}

static int
main_loop(int server_fd)
{
  SDL_Thread *event_thread;
  SDL_Event quit_event = {0};
  unsigned int prev_ms = 0, time_ms, ms;
  int running, flags = FORCE_UPDATE;

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
    profile_end(PROF_IO);
    profile_start(PROF_EVENTS);
    if (SDL_mutexP(event_mutex) < 0)
      die("unable to lock mutex");
    if (event.type != SDL_NOEVENT) {
      if (process_event(&event, time_ms, &flags) < 0)
        running = 0;
      SDL_PumpEvents();
      while (SDL_PollEvent(&event))
        if (process_event(&event, time_ms, &flags) < 0)
          running = 0;
      event.type = SDL_NOEVENT;
      SDL_CondSignal(event_cond);
    }
    if (SDL_mutexV(event_mutex) < 0)
      die("unable to unlock mutex");
    profile_end(PROF_EVENTS);
    ms = SDL_GetTicks();
    if (ms - prev_ms >= frame_ms) {
      int t;
      profile_start(PROF_UPDATE);
      t = uifs_update(flags & FORCE_UPDATE);
      profile_end(PROF_UPDATE);
      if (t)
        draw(flags & FORCE_REDRAW);
      flags = 0;
      prev_ms = ms;
    }
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
sdl_init()
{
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    log_printf(LOG_ERR, "SDL_Init failed.\n");
    return -1;
  }
  atexit(SDL_Quit);
  SDL_ShowCursor(show_cursor);
  if (init_screen())
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
        case 'f': logmask |= LOG_FRONT; break;
        }
    else if (!strcmp(argv[i], "-s") && i + 1 < argc) {
      if (sscanf(argv[++i], "%dx%d", &screen_w, &screen_h) != 2)
        screen_w = screen_h = 0;
    } else if (!strcmp(argv[i], "-f") && i + 1 < argc) {
      if (sscanf(argv[++i], "%d", &j) != 1 || j <= 0)
        die("Wrong frames per second.");
      moveptr_events_interval_ms = frame_ms = 1000 / j;
    } else if (!strcmp(argv[i], "-nocursor"))
      show_cursor = 0;
    else if (!strcmp(argv[i], "-ndbuf"))
      sdl_flags &= ~SDL_DOUBLEBUF;
    else if (!strcmp(argv[i], "-nhw"))
      sdl_flags &= ~SDL_HWSURFACE;
    else if (!strcmp(argv[i], "-naf"))
      sdl_flags &= ~SDL_ANYFORMAT;
    else
      die("Usage: uifs [-d logmask] [-s WxH] [-nocursor]");
  log_printf(LOG_CLIENT, "logging: client\n");
  log_printf(LOG_DATA, "logging: data\n");
  log_printf(LOG_DBG, "logging: dbg\n");
  log_printf(LOG_MSG, "logging: msg\n");
  log_printf(LOG_UI, "logging: ui\n");
  log_printf(LOG_FRONT, "logging: front\n");
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
  if (sdl_init())
    die("Cannot init SDL (%s).", SDL_GetError());
  main_loop(fd);
  close(fd);
  free_screen();
  free_network();
  return 0;
}
