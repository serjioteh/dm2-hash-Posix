#include "Server.h"

int Server::mksock(const char *addr, int port)
{
	int i;
	struct sockaddr_in serv;

	socket_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_sock == -1)
        throw std::system_error(errno, std::system_category());

	i = 1;
	if (setsockopt(socket_sock, SOL_SOCKET, SO_REUSEADDR, (void *)&i, (socklen_t)sizeof(i)) == -1)
		warn("setsockopt");

	memset(&serv, 0, sizeof(struct sockaddr_in));
	serv.sin_family = AF_INET;
	serv.sin_port = htons(port);
	serv.sin_addr.s_addr = inet_addr(addr);

	i = bind(socket_sock, (struct sockaddr *)&serv, (socklen_t)sizeof(serv));
	if (i == -1)
        throw std::system_error(errno, std::system_category());

	set_nonblock(socket_sock);

	i = listen(socket_sock, SOMAXCONN);
	if (i == -1)
        throw std::system_error(errno, std::system_category());

	return(socket_sock);
}

Server::Server(uint16_t port, int *fd_passing) : unix_fd_writers(fd_passing)
{
	/* get a listening socket */
	const char *addr = "127.0.0.1";
	binded_socket = mksock(addr, port);

	/* get our kqueue descriptor */
	kq = kqueue();
	if (kq == -1)
        throw std::system_error(errno, std::system_category());

	memset(&ke, 0, sizeof(struct kevent));

	/* fill out the kevent struct */
	EV_SET(&ke, binded_socket, EVFILT_READ, EV_ADD, 0, 5, NULL);

	/* set the event */
	int i = kevent(kq, &ke, 1, NULL, 0, NULL);
	if (i == -1)
        throw std::system_error(errno, std::system_category());
}

void Server::start()
{
    while (1)
    {
		memset(&ke, 0, sizeof(ke));
		
		/* receive an event, a blocking call as timeout is NULL */
		int i = kevent(kq, NULL, 0, &ke, 1, NULL);

		if (i == -1)
	        throw std::system_error(errno, std::system_category());

		if (i == 0)
			continue;

		/*
		 * since we only have one kevent in the eventlist, we're only
		 * going to get one event at a time
		 */

		if (ke.ident == binded_socket)
		{
			/* server socket, theres a client to accept */
			socklen_t len = (socklen_t)sizeof(c);
			int SlaveSocket = accept(binded_socket, (struct sockaddr *)&c, &len);
			if (SlaveSocket == -1)
		        throw std::system_error(errno, std::system_category());

			
		    if (SlaveSocket > 0)
		    {
				set_nonblock(SlaveSocket);
		        int i = send_fd(unix_fd_writers[rand() % NUM_CHILDS], SlaveSocket);
				if (i == -1)
			        throw std::system_error(errno, std::system_category());				
		    }
		}
	}
}

Server::~Server()
{
    EV_SET(&ke, binded_socket, EVFILT_READ, EV_DELETE, 0, 0, NULL);
    int i = kevent(kq, &ke, 1, 0, 0, NULL);
    if (i == -1)
        throw std::system_error(errno, std::system_category());

    close(binded_socket);
    shutdown(socket_sock, SHUT_RDWR);
    close(socket_sock);

    for (int i = 0; i < NUM_CHILDS; i++)
        close(unix_fd_writers[i]);
}

