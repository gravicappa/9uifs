#ifdef WIN32
#include <winsock2.h>
#define socklen_t int
#else
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#endif

int init_network(void);
void free_network(void);
int net_listen(const char *host, int port);
int nonblock_socket(int s);
int net_wouldblock();
