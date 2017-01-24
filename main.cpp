#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
	
#include <set>
#include <sys/stat.h>

#include <signal.h>
#include <sys/wait.h>

#include "Server.h"
#include "Child.h"

Server *p_server;
Child *p_child;

pid_t pid_srv = getpid(), pid_wrk;

int shmfd;
void *shmaddr;

sem_t *sems[HashTable::NUM_SECTIONS];

std::set<pid_t> childs_pids;

void create_shared_memory()
{
	struct stat mapstat;
	
    if ((shmfd = shm_open("memory_shared", O_CREAT | O_RDWR, 0777)) == -1)
        throw std::system_error(errno, std::system_category());
	
	std::cout << "shm_open ok" << std::endl;
	  
	if (-1 != fstat(shmfd, &mapstat) && mapstat.st_size == 0) // MacOSX specific behavior - ftruncate could be run only once after segment creation
	{
        std::cout << "shared memory size before ftrancate: " << mapstat.st_size << std::endl;

        if (ftruncate(shmfd, HashTable::TOTAL_MEM_SIZE) == -1)
		{
			std::cout << "ftrancate error" << std::endl;
	        throw std::system_error(errno, std::system_category());
		}
	
		std::cout << "ftrancate ok" << std::endl;
        if (-1 != fstat(shmfd, &mapstat)) // MacOSX specific behavior - ftruncate could be run only once after segment creation
        {
            std::cout << "shared memory size after ftrancate: " << mapstat.st_size << std::endl;
        }
        else
            throw std::system_error(errno, std::system_category());

	}
	

    if ((shmaddr = mmap(0, HashTable::TOTAL_MEM_SIZE, PROT_WRITE, MAP_SHARED, shmfd, 0)) == MAP_FAILED)
        throw std::system_error(errno, std::system_category());
	
	std::cout << "mmap ok" << std::endl;
}

void close_shared_memory()
{
    munmap(shmaddr, HashTable::TOTAL_MEM_SIZE);
    close(shmfd);
}

void delete_shared_memory()
{
    std::cout << "shm_deleted" << std::endl;
    shm_unlink("memory_shared");
}

void create_semaphors()
{
    for (int i = 0; i < HashTable::NUM_SECTIONS; i++)
    {
        char temp[13];
        sprintf(temp, "/sem%03d", i);
        sems[i] = sem_open(temp, O_CREAT, 0666, 1);
    }
}

void close_semaphors()
{
    for (int i = 0; i < HashTable::NUM_SECTIONS; i++)
        sem_close(sems[i]);
}

void delete_semaphors()
{
    for (int i = 0; i < HashTable::NUM_SECTIONS; i++)
    {
        char temp[13];
        sprintf(temp, "/sem%03d", i);
        sem_unlink(temp);
    }
}

void handler(int signum)
{
    signal(signum, &handler);

    if (signum == SIGINT || signum == SIGTERM)
    {
        if (pid_srv == getpid())
        {
            for (pid_t pid : childs_pids)
                kill(pid, signum);
            while (wait(NULL) > 0);
            delete p_server;

            delete_semaphors();
            delete_shared_memory();
            exit(0);
        }
        else
        {
            if (p_child) delete p_child;

            close_semaphors();
            close_shared_memory();
            exit(0);
        }
    }
}

class HashTableCleaner : public HashTable
{
public:
    HashTableCleaner(void *shmaddr) : HashTable(shmaddr) {}

    void clean()
    {
        int step = (HASH_TABLE_SIZE - 1) / NUM_SECTIONS;

        size_t i = 0;
        for (int j = 0; j < NUM_SECTIONS; j++)
        {
            sem_wait(sems[j]);

            for ( ; i / NUM_SECTIONS == j && i < NUM_SECTIONS; i++)
            {
                NodePtr p_node(p_data, p_table[i]);
                while (!p_node.is_null())
                {
                    NodePtr p_node_free = p_node;
                    p_node = p_node.get_next();

                    if (p_node_free.is_expired())
                        set_node_free(p_node_free);
                }
            }

            sem_post(sems[j]);
        }
    }
};

int main()
{
    signal(SIGTERM, &handler);
    signal(SIGINT, &handler);

    int unix_fd_writers[NUM_CHILDS];

    create_shared_memory();
    HashTable::shared_memory_set(shmaddr);
    close_shared_memory();
    create_semaphors();
    close_semaphors();
	

    try
    {
        for (int i = 0; i < NUM_CHILDS; i++)
        {
            int unix_sockets[2];

            socketpair(AF_LOCAL, SOCK_STREAM, 0, unix_sockets);
            set_nonblock(unix_sockets[0]);
            set_nonblock(unix_sockets[1]);

            if ((pid_wrk = fork()) == 0)
            {
                close(unix_sockets[0]);

                create_shared_memory();
                create_semaphors();
				
                p_child = new Child(shmaddr, sems, unix_sockets[1]);
				std::cout << "Child created" << std::endl;
				
                p_child->start();
				
                delete p_child;

                close_semaphors();
                close_shared_memory();
            }
            else
            {
                close(unix_sockets[1]);
                unix_fd_writers[i] = unix_sockets[0];
                childs_pids.insert(pid_wrk);
            }
        }

        if ((pid_wrk = fork()) > 0)
        {
            childs_pids.insert(pid_wrk);

            p_server = new Server(3100, unix_fd_writers);
            p_server->start();
            delete p_server;
        }
        else
        {
            create_shared_memory();
            create_semaphors();

            HashTableCleaner hash_table_cleaner(shmaddr);

            while (1)
            {
                sleep(60);
                hash_table_cleaner.clean();
            }
        }
    }
    catch (const std::system_error& error)
    {
        if (p_server)
        {
            delete p_server;
            p_server = NULL;
        }

        if (p_child)
        {
            delete p_child;
            p_child = NULL;

            close_semaphors();
            close_shared_memory();
        }

        std::cout << "Error: " << error.code()
                  << " - " << error.code().message() << '\n';
    }
    catch (char const * error)
    {
        if (p_server)
        {
            delete p_server;
            p_server = NULL;
        }

        if (p_child)
        {
            delete p_child;
            p_child = NULL;

            close_semaphors();
            close_shared_memory();
        }

        std::cout << error << std::endl;
    }

    if (getpid() == pid_wrk)
    {
        delete_semaphors();
        delete_shared_memory();
    }

    return 0;
}