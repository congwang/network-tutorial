CC=gcc
CFLAGS=-Wall
LIBS_SCTP=-lsctp

all: sctp_server sctp_client

sctp_server: sctp_server.c
	$(CC) $(CFLAGS) -o sctp_server sctp_server.c $(LIBS_SCTP)

sctp_client: sctp_client.c
	$(CC) $(CFLAGS) -o sctp_client sctp_client.c $(LIBS_SCTP)

clean:
	rm -f sctp_server sctp_client