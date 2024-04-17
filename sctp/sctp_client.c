#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/sctp.h>
#include <arpa/inet.h>

#define SERVER_PORT  8080
#define BUFFER_SIZE  1024

int main() {
    int conn_fd;
    struct sockaddr_in servaddr;
    struct sctp_sndrcvinfo sndrcvinfo;
    char buffer[BUFFER_SIZE];
    int flags;

    // Creating SCTP TCP-Style Socket
    conn_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
    if (conn_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Initialize server address structure
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &(servaddr.sin_addr));
    servaddr.sin_port = htons(SERVER_PORT);

    // Connect to the server
    if (connect(conn_fd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("connect");
        close(conn_fd);
        exit(EXIT_FAILURE);
    }

    // Send a message to the server
    strcpy(buffer, "Hello, Server!");
    if (sctp_sendmsg(conn_fd, (void *)buffer, (size_t)strlen(buffer),
                     NULL, 0, 0, 0, 0, 0, 0) < 0) {
        perror("sctp_sendmsg");
        close(conn_fd);
        exit(EXIT_FAILURE);
    }

    // Receive the echo
    flags = 0;
    if (sctp_recvmsg(conn_fd, (void *)buffer, sizeof(buffer),
                     (struct sockaddr *)NULL, 0, &sndrcvinfo, &flags) < 0) {
        perror("sctp_recvmsg");
        close(conn_fd);
        exit(EXIT_FAILURE);
    }

    // Print the received data
    printf("Received from server: %s\n", buffer);

    close(conn_fd);
    return 0;
}