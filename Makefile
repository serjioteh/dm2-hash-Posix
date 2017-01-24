CC=g++
CFLAGS=-std=c++11

SRC_SERVER=Server.cpp
SRC_WORKER=Child.cpp
SRC_HASH=HashTable.cpp
SRC_ROUTINE=snd_rcv_fd.cpp
SRC_ERROR=error.c

all:
	$(CC) main.cpp $(SRC_SERVER) $(SRC_WORKER) $(SRC_HASH) $(SRC_ROUTINE) $(SRC_ERROR) -o server $(CFLAGS)

clean:
	rm -f *.o server

