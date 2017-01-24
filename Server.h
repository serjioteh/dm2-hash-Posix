#ifndef SERVER_H
#define SERVER_H

#include "MyHeader.h"

class Server
{
public:
    Server(uint16_t port, int *fd_passing);
	int  mksock(const char *addr, int port);

    void start();

    ~Server();
private:
    int *unix_fd_writers;

    int    binded_socket, kq, socket_sock;
	struct sockaddr_in c; /* client */
	struct kevent ke;
};

#endif //SERVER_H
