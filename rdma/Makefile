CC=gcc
CFLAGS=-Wall
LIBS_VERBS=-libverbs

all: epoll liburing libevent zerocopy

epoll: epoll.c
	$(CC) $(CFLAGS) -o epoll epoll.c

liburing: liburing.c
	$(CC) $(CFLAGS) -o liburing liburing.c $(LIBS_URING)

libevent: libevent.c
	$(CC) $(CFLAGS) -o libevent libevent.c $(LIBS_EVENT)

zerocopy: zerocopy.c
	$(CC) $(CFLAGS) -o zerocopy zerocopy.c

rdma: rdma.c
	$(CC) $(CFLAGS) -o rdma rdma.c $(LIBS_VERBS)

sctp:
	$(MAKE) -C sctp

clean:
	rm -f epoll liburing libevent zerocopy rdma
	$(MAKE) -C sctp clean