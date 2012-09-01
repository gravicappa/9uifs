#include <SDL/SDL.h>
#include <Imlib2.h>

#ifdef WIN32
#include <winsock2.h>
#define socklen_t int
#else
#include <sys/select.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#endif

#include "9p.h"
#include "net.h"
#include "util.h"
#include "fs.h"
#include "event.h"
#include "client.h"
#include "draw.h"

int server_fd = -1;
int server_port = 5558;
int scr_w = 320;
int scr_h = 200;
int frame_ms = 1000 / 30;
char *server_host = 0;

int
main_loop(int server_fd)
{
  SDL_Event ev;
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
        client_pointer_move(ev.motion.x, ev.motion.y, ev.motion.state);
        break;
      case SDL_MOUSEBUTTONDOWN:
        client_pointer_press(1, ev.button.x, ev.button.y, ev.button.button);
        break;
      case SDL_MOUSEBUTTONUP:
        client_pointer_press(0, ev.button.x, ev.button.y, ev.button.button);
        break;
      case SDL_KEYDOWN:
        client_keyboard(1, ev.key.keysym.sym, ev.key.keysym.mod,
                        ev.key.keysym.unicode);
        break;
      case SDL_KEYUP:
        client_keyboard(0, ev.key.keysym.sym, ev.key.keysym.mod,
                        ev.key.keysym.unicode);
        if (ev.key.keysym.sym == SDLK_ESCAPE
            && (ev.key.keysym.mod & (KMOD_LCTRL | KMOD_RCTRL)))
          running = 0;
        break;
      }
    }
    if (process_clients(server_fd, time_ms))
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
    else
      die("Usage: d [-d logmask]");
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
