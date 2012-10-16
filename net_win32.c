#include <winsock2.h>

#include "util.h"

int
init_network(void)
{
  WORD ver;
  WSADATA data;
  int err;

  ver = MAKEWORD(2, 0);
  err = WSAStartup(ver, &data);
  if (err)
    log_printf(LOG_ERR, "WSAStartup failed with error: %d\n", err);
  return err != 0;
}

void
free_network(void)
{
  WSACleanup();
}

int
nonblock_socket(int s)
{
  unsigned long nonblock = 1;
  if (ioctlsocket(s, FIONBIO, &nonblock) == SOCKET_ERROR)
    return -1;
  return 0;
}

int
net_wouldblock(void)
{
  return WSAGetLastError() == WSAEWOULDBLOCK;
}
