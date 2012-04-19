#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#ifdef WIN32
#include <winsock2.h>
#define socklen_t int
#else
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#endif

#include "util.h"

int
ip_from_str(const char *addr)
{
  struct addrinfo hints, *result = 0;
  struct sockaddr_in *ip;
  int ret = INADDR_ANY;

  if (!addr || strcmp(addr, "any") == 0)
    return ret;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = 0;
  hints.ai_protocol = 0;
  hints.ai_canonname = 0;
  hints.ai_addr = 0;
  hints.ai_next = 0;
  if (getaddrinfo(addr, 0, &hints, &result) != 0)
    die("Cannot get target host address.");
  if (!result || result->ai_addrlen > sizeof(struct sockaddr_in)) {
    if (result)
      freeaddrinfo(result);
    die("Unsupported target host address.");
  }
  ip = (struct sockaddr_in *)result->ai_addr;
  ret = ip->sin_addr.s_addr;
  if (result)
    freeaddrinfo(result);
  return ret;
}

int
net_listen(const char *host, int port)
{
  int s, f;
  struct sockaddr_in addr;

  s = socket(AF_INET, SOCK_STREAM, 0);
  if (s == -1)
    return -1;

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = host ? ip_from_str(host) : INADDR_ANY;

  f = 1;
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &f, sizeof(f));
  f = 1;
  setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, &f, sizeof(f));

  if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    die("Cannot bind address.");

  if (listen(s, 10) == -1)
    die("Cannot listen for connections.");
  return s;
}
