#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "util.h"

int
init_network(void)
{
  return 0;
}

void
free_network(void)
{
}

int
nonblock_socket(int s)
{
  int flags;
  flags = fcntl(s, F_GETFL, 0);
  if (flags < 0 || fcntl(s, F_SETFL, flags | O_NONBLOCK) < 0)
    return -1;
  return 0;
}

int
net_wouldblock(void)
{
  return errno == EWOULDBLOCK || errno == EAGAIN;
}
