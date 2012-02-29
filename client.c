#include <string.h>
#include <stdlib.h>

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
#include "9p.h"
#include "9pio.h"
#include "fs.h"
#include "client.h"
#include "net.h"

struct buf clients = {16 * sizeof(struct client)};

struct client *
add_client(int server_fd, int msize)
{
  struct client *c;
  int off, fd;
  struct sockaddr_in addr;
  socklen_t addr_len;

  addr_len = sizeof(addr);
  fd = accept(server_fd, (struct sockaddr *)&addr, &addr_len);
  if (fd < 0)
    return 0;
  log_printf(3, "; Incoming connection\n");
  off = add_data(&clients, sizeof(struct client), 0);
  if (off < 0)
    die("Cannot allocate memory\n");
  c = (struct client *)(clients.b + off);
  c->fd = fd;
  c->read = 0;
  c->size = 0;
  c->msize = msize;
  c->inbuf = (char *)malloc(msize);
  c->outbuf = (char *)malloc(msize);

  return c;
}

void
rm_client(struct client *c)
{
  int off, end;

  log_printf(3, "; rm_client %p\n", c);

  if (!c)
    return;
  if (c->inbuf)
    free(c->inbuf);
  if (c->outbuf)
    free(c->outbuf);
  off = c - (struct client *)clients.b;
  end = off + sizeof(struct client);
  memmove(clients.b + off, clients.b + end, clients.used - end);
  clients.used -= sizeof(struct client);
}

unsigned int
unpack_uint4(unsigned char *buf)
{
  return buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
}

int
process_client(struct client *c)
{
  int r;
  unsigned int size, outsize, msize = c->msize;
  char *inbuf = c->inbuf, *outbuf = c->outbuf;

  r = recv(c->fd, inbuf + c->read, msize - c->read, 0);
  if (r <= 0)
    return -1;
  c->read += r;
  if (c->read < 4)
    return 0;
  do {
    log_printf(3, "; <- ");
    log_print_data(3, c->read, (unsigned char *)inbuf);
    size = unpack_uint4((unsigned char *)inbuf);
    log_printf(3, ";   size: %d\n", size);
    if (size < 7 || size > msize)
      return -1;
    log_printf(3, ";   c->read: %d\n", c->read);
    if (size > c->read)
      return 0;
    if (p9_process_srv(msize, c->inbuf, msize, c->outbuf, &c->c, &fs))
      return -1;
    outsize = unpack_uint4((unsigned char *)c->outbuf);
    log_printf(3, "; -> ");
    log_print_data(3, outsize, (unsigned char *)outbuf);
    if (send(c->fd, outbuf, outsize, 0) <= 0)
      return -1;
    memmove(inbuf, inbuf + size, msize - size);
    c->read -= size;
  } while (1);
}
