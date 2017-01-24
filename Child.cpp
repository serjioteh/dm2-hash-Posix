#include "Child.h"


Child::Child(void *shmaddr, sem_t **sems, int fd_passing) :
        unix_fd_reader(fd_passing), semids(sems), hash_table(shmaddr)
{
	/* get our kqueue descriptor */
	kq = kqueue();
	if (kq == -1)
        throw std::system_error(errno, std::system_category());

	memset(&ke, 0, sizeof(struct kevent));

	/* fill out the kevent struct */
	EV_SET(&ke, unix_fd_reader, EVFILT_READ, EV_ADD, 0, 5, NULL);

	/* set the event */
	int i = kevent(kq, &ke, 1, NULL, 0, NULL);
	if (i == -1)
        throw std::system_error(errno, std::system_category());
	
}

void Child::start()
{
    while (1)
    {
		memset(&ke, 0, sizeof(ke));
		
		/* receive an event, a blocking call as timeout is NULL */
		int i = kevent(kq, NULL, 0, &ke, 1, NULL);

		if (i == -1)
		{
			std::cout << "child start failed" << std::endl;			
	        throw std::system_error(errno, std::system_category());
		}

		if (i == 0)
			continue;

        if (ke.ident == unix_fd_reader)
		{
			SlaveSocket = recv_fd(unix_fd_reader);
			memset(&ke, 0, sizeof(ke));
			EV_SET(&ke, SlaveSocket, EVFILT_READ, EV_ADD | EV_EOF, 0, 0, NULL);
			int i = kevent(kq, &ke, 1, NULL, 0, NULL);
			if (i == -1)
		        throw std::system_error(errno, std::system_category());

		    slave_sockets.insert(SlaveSocket);
		    std::cout << "DEBUG: " << "fd " << SlaveSocket  << " connected" << std::endl;
		    send(SlaveSocket, "Welcome!\n", 9, MSG_NOSIGNAL);
			
		}
        else
		{   
			// client event
		    int client_fd = ke.ident;

		    if (recv_message_from_client(client_fd))
		    {
		        Query query;
		        int res;

		        if ((res = parse_query(client_fd, &query)) != 0)
		        {
		            if (res > 0)
                        run_query(query);

		            send_message_to_client(client_fd);
		        }
		    }
    	}
    	
	}
}

Child::~Child()
{

    for (auto client: slave_sockets)
        disconnect_client(client);

    close(unix_fd_reader);
}


char Child::parse_query(int client_fd, Query *p_query)
{
    char cmd[32];
    int key, ttl, value, n_args;

    if ((n_args = sscanf(buffer, "%s%d%d%d", cmd, &ttl, &key, &value)) <= 0)
        return 0;
	
	if (n_args == 2)
		key = ttl;

    p_query->key = key;
    p_query->value = value;
    p_query->from = client_fd;
    p_query->ttl = ttl;

    std::cout << "[CMD from " << client_fd << "]:";

    if (strcmp(cmd, "set") == 0 && n_args == 4)
    {
        p_query->op = SET;
    }
    else if (strcmp(cmd, "get") == 0 && n_args == 2)
    {
        p_query->op = GET;
    }
    else if (strcmp(cmd, "del") == 0 && n_args == 2)
    {
        p_query->op = DEL;
    }
    else
    {
        std::cout << buffer;

        std::cout << "DEBUG: Command from " << client_fd << " is FAILED: "
                  << "Wrong command or wrong number of params" << std::endl;

        sprintf(buffer, "> ERROR: Wrong command or wrong number of params\n");
        return -1;
    }

    if (n_args >= 1)
    {
        std::cout << "cmd: " << cmd;
        if (n_args >= 2)
        {
            std::cout << " ttl: " << ttl;
            if (n_args >= 4)
                std::cout << " key: " << key << " value: " << value;
        }
    }
    std::cout << std::endl;
    return 1;
}

void Child::run_query(Query query)
{
    int sem_id = hash_table.get_hash(query.key) / HashTable::NUM_SECTIONS;

    sem_down(sem_id);

    try
    {
        switch (query.op)
        {
            case SET:
                hash_table.set(query.key, query.ttl, query.value);
                break;

            case GET:
                query.value = hash_table.get(query.key);
                break;

            case DEL:
                hash_table.del(query.key);
                break;

            default: ;
        }

        std::cout << "DEBUG: Command from " << query.from << " is DONE" << std::endl;
		
		if (query.op == GET)
            sprintf(buffer, "RESULT: (%d , %d)\n", query.key, query.value);
		else
            sprintf(buffer, "DONE.\n");
    }
    catch (HashTableError err)
    {
        switch (err.getType())
        {
            case HashTableErrorType::NoKey:
                std::cout << "DEBUG: Command from " << query.from << " is FAILED: "
                          << "There is no key " << query.key << std::endl;
                sprintf(buffer, "ERROR: There is no key %d\n", query.key);
                break;

            case HashTableErrorType::NoMemory:
                std::cout << "DEBUG: Command from " << query.from << " is FAILED: "
                          << "There is no more free memory left" << std::endl;
                sprintf(buffer, "ERROR: There is no more free memory left\n");
                break;

            case HashTableErrorType::NotExists:
                std::cout << "DEBUG: Command from " << query.from << " is FAILED: "
                          << "Element with key " << query.key << " is expired" << std::endl;
                sprintf(buffer, "ERROR: Element with key %d is expired\n", query.key);
                break;
        }
    }

    sem_up(sem_id);
}

bool Child::recv_message_from_client(int client_fd)
{
    ssize_t recv_size = read(client_fd, buffer, MAX_BUF_SIZE);

    if (recv_size > 0)
    {
        buffer[recv_size] = 0;
        return true;
    }
    else if (recv_size == 0)
    {
        disconnect_client(client_fd);
        return false;
    }
    else
    {
        throw std::system_error(errno, std::system_category());
    }
}

void Child::send_message_to_client(int client_fd)
{
    send(client_fd, buffer, strlen(buffer) + 1, MSG_NOSIGNAL);
}

void Child::disconnect_client(int client_fd)
{
	EV_SET(&ke, client_fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
	int i = kevent(kq, &ke, 1, 0, 0, NULL);
	if (i == -1)
        throw std::system_error(errno, std::system_category());

    slave_sockets.erase(client_fd);

    shutdown(client_fd, SHUT_RDWR);
    close(client_fd);

    std::cout << "DEBUG: " << client_fd  << " disconnected" << std::endl;
}

