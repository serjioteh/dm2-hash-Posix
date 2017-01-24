#ifndef MYHEADER_H
#define MYHEADER_H

#include <iostream>
#include <algorithm>
#include <set>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/event.h>

#include <errno.h>
#include <err.h>
#include <string.h>

// MSG_NOSIGNAL does not exists on OS X
#if defined(__APPLE__) || defined(__MACH__)
# ifndef MSG_NOSIGNAL
#   define MSG_NOSIGNAL SO_NOSIGPIPE
# endif
#endif


static const size_t MAX_BUF_SIZE = 1024;
static const size_t NUM_CHILDS = 4;

int set_nonblock(int fd);

int send_fd(int fd, int fd_to_send);
int recv_fd(int fd);


#endif //MYHEADER_H
