CC=gcc
CFLAGS=-Wall
LIBS_VERBS=-libverbs

all: rdma

sctp_server: rdma.c
	$(CC) $(CFLAGS) -o rdma rdma.c $(LIBS_VERBS)

clean:
	rm -f rdma