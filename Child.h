#ifndef CHILD_H
#define CHILD_H

#include <semaphore.h>

#include "MyHeader.h"
#include "HashTable.h"

enum op_type { GET, SET, DEL };

struct Query
{
    int key, value, from, ttl;
    op_type op;
};

class Child
{
public:
    Child(void *shmaddr, sem_t **sems, int fd_passing);

    void start();

    ~Child();

private:
    char parse_query(int client_fd, Query *p_query);
    void run_query(Query query);

    bool recv_message_from_client(int client_fd);
    void send_message_to_client(int client_fd);

    void disconnect_client(int client_fd);

    void sem_up(int i, int c=1)
    {
        for (int j = 0; j < c; j++)
            sem_wait(semids[i]);
    }

    void sem_down(int i, int c=1)
    {
        for (int j = 0; j < c; j++)
            sem_post(semids[i]);
    }

private:	
    int unix_fd_reader;

    int    kq, SlaveSocket;
	struct sockaddr_in c; /* client */
	struct kevent ke;

    std::set<int> slave_sockets;

    char buffer[MAX_BUF_SIZE+1];

    HashTable hash_table;
    sem_t **semids;
};

#endif //CHILD_H
