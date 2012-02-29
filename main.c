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
#include "client.h"

int server_fd = -1;
int server_port = 5558;
char *server_host = 0;

int
update_sock_set(fd_set *fdset, int server_fd)
{
  struct client *c = (struct client *)clients.b;
  int m, i, nclients;

  FD_ZERO(fdset);
  FD_SET(server_fd, fdset);

  nclients = clients.used / sizeof(struct client);
  m = server_fd;
  for (i = 0; i < nclients; ++i) {
    FD_SET(c[i].fd, fdset);
    m = (m > c[i].fd) ? m : c[i].fd;
    log_printf(5, "; client[%d] fd: %d\n", i, c[i].fd);
  }
  log_printf(5, "; fd: %d m: %d\n", server_fd, m);
  return m;
}

int
main_loop(int server_fd)
{
  fd_set fdset;
  SDL_Event ev;
  int m, i, r, nclients, running = 1;
  struct client *c;

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
      nclients = clients.used / sizeof(struct client);
      c = (struct client *)clients.b;
      log_printf(9, ";;   c: %p b: %p n: %d\n", c, clients.b, nclients);
      for (i = 0; i < nclients; ++i) {
        log_printf(9, ";;   checking fd: %d\n", c[i].fd);
        if (FD_ISSET(c[i].fd, &fdset))
          if (process_client(&c[i]))
            rm_client(&c[i]);
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
      die("usage: d [-d loglevel]");
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
