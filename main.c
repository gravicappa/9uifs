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
char *server_host = 0;

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
  struct client *c, *cnext;

  while (running) {
    while (SDL_PollEvent(&ev)) {
      switch (ev.type) {
      case SDL_QUIT:
        running = 0;
      }
    }
    m = update_sock_set(&fdset, server_fd);
    r = select(m + 1, &fdset, 0, 0, 0);
    if (r < 0)
      break;
    if (r > 0) {
      if (FD_ISSET(server_fd, &fdset))
        add_client(server_fd, 65536);
      for (c = clients; c; c = cnext) {
        cnext = c->next;
        if (FD_ISSET(c->fd, &fdset))
          if (process_client(c))
            rm_client(c);
      }
    }
  }
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
  if (SDL_Init(0) < 0)
    die("Cannot init SDL");
  main_loop(fd);
  SDL_Quit();
  return 0;
}
