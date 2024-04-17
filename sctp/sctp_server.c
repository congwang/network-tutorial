#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/sctp.h>

#define SERVER_PORT  8080
#define BUFFER_SIZE  1024
#define LISTEN_QUEUE  5

int main() {
    int listen_fd, conn_fd;
    struct sockaddr_in servaddr;
    struct sctp_sndrcvinfo sndrcvinfo;
    struct sctp_event_subscribe events;
    char buffer[BUFFER_SIZE];
    int flags;

    // Creating SCTP TCP-Style Socket
    listen_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
    if (listen_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Initialize server address structure
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(SERVER_PORT);

    // Binding
    if (bind(listen_fd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("bind");
        close(listen_fd);
        exit(EXIT_FAILURE);
    }

    // Setting SCTP events
    memset(&events, 0, sizeof(events));
    events.sctp_data_io_event = 1;
    if (setsockopt(listen_fd, SOL_SCTP, SCTP_EVENTS, &events, sizeof(events)) < 0) {
        perror("setsockopt");
        close(listen_fd);
        exit(EXIT_FAILURE);
    }

    // Listen with a queue length of LISTEN_QUEUE
    if (listen(listen_fd, LISTEN_QUEUE) < 0) {
        perror("listen");
        close(listen_fd);
        exit(EXIT_FAILURE);
    }

    // Server loop...
    for (;;) {
        // Accept a new connection
        conn_fd = accept(listen_fd, NULL, NULL);
        if (conn_fd < 0) {
            perror("accept");
            continue;   // Just log and wait for the next connection request
        }

        // Clear the buffer
        bzero(buffer, BUFFER_SIZE);

        // Receive a message from the client
        flags = 0;
        if (sctp_recvmsg(conn_fd, (void *)buffer, BUFFER_SIZE, 
                        (struct sockaddr *)NULL, 0, &sndrcvinfo, &flags) < 0) {
            perror("sctp_recvmsg");
            close(conn_fd);
            continue;
        }

        // Echo the received data back
        if (sctp_sendmsg(conn_fd, (void *)buffer, (size_t)strlen(buffer),
                        NULL, 0, sndrcvinfo.sinfo_ppid,
                        0, sndrcvinfo.sinfo_stream, 0, 0) < 0) {
            perror("sctp_sendmsg");
            close(conn_fd);
            continue;
        }

        // Close the connection
        close(conn_fd);
    }

    // Should never reach this point
    close(listen_fd);
    return 0;
}