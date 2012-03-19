#include <SDL/SDL.h>

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
#include "client.h"

int server_fd = -1;
int server_port = 5558;
int scr_w = 320;
int scr_h = 200;
char *server_host = 0;

SDL_Surface *screen = 0;
SDL_Surface *back_screen = 0;

int
update_sock_set(fd_set *fdset, int server_fd)
{
  struct client *c = clients;
  int m;

  FD_ZERO(fdset);
  FD_SET(server_fd, fdset);

  m = server_fd;
  for (c = clients; c; c = c->next) {
    FD_SET(c->fd, fdset);
    m = (m > c->fd) ? m : c->fd;
  }
  return m;
}

int
main_loop(int server_fd)
{
  fd_set fdset;
  SDL_Event ev;
  int m, r, running = 1;
  unsigned int key;
  struct client *c, *cnext;
  struct timeval tv;

  while (running) {
    while (SDL_PollEvent(&ev)) {
      log_printf(3, "#SDL ev.type: %d\n", ev.type);
      switch (ev.type) {
      case SDL_QUIT:
        running = 0;
        break;
      case SDL_MOUSEMOTION:
        client_pointer_move(ev.motion.x, ev.motion.y, ev.motion.state);
        break;
      case SDL_MOUSEBUTTONDOWN:
        client_pointer_click('d', ev.button.x, ev.button.y, ev.button.button);
        break;
      case SDL_MOUSEBUTTONUP:
        client_pointer_click('u', ev.button.x, ev.button.y, ev.button.button);
        break;
      case SDL_KEYDOWN:
        client_keyboard('d', ev.key.keysym.sym, ev.key.keysym.mod,
                        ev.key.keysym.unicode);
        break;
      case SDL_KEYUP:
        client_keyboard('u', ev.key.keysym.sym, ev.key.keysym.mod,
                        ev.key.keysym.unicode);
        break;
      }
    }
    m = update_sock_set(&fdset, server_fd);
    tv.tv_sec = 0;
    tv.tv_usec = 33000;
    r = select(m + 1, &fdset, 0, 0, &tv);
    if (r < 0)
      break;
    if (r > 0) {
      if (FD_ISSET(server_fd, &fdset))
        add_client(server_fd, 65536);
      for (c = clients; c; c = cnext) {
        cnext = c->next;
        if (FD_ISSET(c->fd, &fdset))
          if (process_client_io(c))
            rm_client(c);
      }
    }
  }
  return 0;
}

int
sdl_init(int w, int h)
{
  unsigned int flags[] = {
    SDL_HWSURFACE | SDL_DOUBLEBUF,
    SDL_HWSURFACE | SDL_ANYFORMAT,
    SDL_DOUBLEBUF,
    0
  };
  int i;

  if (SDL_Init(SDL_INIT_VIDEO) < 0)
    return -1;

  screen = SDL_SetVideoMode(w, h, 0, flags[0]);
  if (!screen)
    return -1;
  return 0;
}

void
parse_args(int argc, char **argv)
{
  int i;

  for (i = 1; i < argc; ++i)
    if (!strcmp(argv[i], "-d") && i + 1 < argc)
      loglevel = atoi(argv[++i]);
    else
      die("Usage: d [-d loglevel]");
}

int
main(int argc, char **argv)
{
  int fd;

  loglevel = 10;
  parse_args(argc, argv);

  fd = net_listen(server_host, server_port);
  if (fd < 0)
    die("Cannot create listening socket");
  if (sdl_init(scr_w, scr_h))
    die("Cannot init SDL");
  main_loop(fd);
  SDL_Quit();
  return 0;
}
