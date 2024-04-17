#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <errno.h>
#include <fcntl.h>

#define MAX_EVENTS 10
#define LISTEN_PORT 8080
#define LISTEN_BACKLOG 50
#define BUFFER_SIZE 512

// Function to set a socket to non-blocking mode
int make_socket_non_blocking(int sfd) {
    int flags = fcntl(sfd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl");
        return -1;
    }

    flags |= O_NONBLOCK;
    if (fcntl(sfd, F_SETFL, flags) == -1) {
        perror("fcntl");
        return -1;
    }
    return 0;
}

int main() {
    int listen_sock, conn_sock, nfds, epollfd;
    struct epoll_event ev, events[MAX_EVENTS];
    struct sockaddr_in listen_addr;
    socklen_t addrlen = sizeof(listen_addr);
    char buffer[BUFFER_SIZE];

    // Create a socket to listen for incoming connections
    listen_sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (listen_sock == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Initialize the listen address structure
    memset(&listen_addr, 0, sizeof(listen_addr));
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_addr.s_addr = INADDR_ANY;
    listen_addr.sin_port = htons(LISTEN_PORT);

    // Bind the listening socket
    if (bind(listen_sock, (struct sockaddr *)&listen_addr, sizeof(listen_addr)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    // Start listening
    if (listen(listen_sock, LISTEN_BACKLOG) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    // The listening socket is already in non-blocking mode thanks to the SOCK_NONBLOCK flag

    // Create an epoll instance
    epollfd = epoll_create1(0);
    if (epollfd == -1) {
        perror("epoll_create1");
        exit(EXIT_FAILURE);
    }

    // Add the listening socket to the epoll instance
    ev.events = EPOLLIN; // Monitor for input
    ev.data.fd = listen_sock;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, listen_sock, &ev) == -1) {
        perror("epoll_ctl: listen_sock");
        exit(EXIT_FAILURE);
    }

    // Event loop
    for (;;) {
        // Wait for events on the epoll file descriptor
        nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        if (nfds == -1) {
            perror("epoll_wait");
            exit(EXIT_FAILURE);
        }

        // Process the available events
        for (int n = 0; n < nfds; ++n) {
            if (events[n].data.fd == listen_sock) {
                // Accept new connections
                while ((conn_sock = accept(listen_sock, (struct sockaddr *)&listen_addr, &addrlen)) > 0) {
                    // Make the incoming socket non-blocking
                    if (make_socket_non_blocking(conn_sock) == -1) {
                        abort(); // Error handling in production code should be more robust
                    }

                    // Set up the new socket for input events and add to epoll
                    ev.events = EPOLLIN | EPOLLET; // Edge-triggered input
                    ev.data.fd = conn_sock;
                    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, conn_sock, &ev) == -1) {
                        perror("epoll_ctl: conn_sock");
                        exit(EXIT_FAILURE);
                    }
                }
                if (conn_sock == -1) {
                    if (errno != EAGAIN && errno != EWOULDBLOCK) {
                        perror("accept");
                        // Handle error
                    }
                    continue; // No more incoming connections
                }
            } else {
                // Handle read/write on the connection socket
                int fd = events[n].data.fd;

                // Read data
                ssize_t count = read(fd, buffer, sizeof(buffer));
                if (count == -1) {
                    // If errno == EAGAIN, we've read all data
                    if (errno != EAGAIN) {
                        perror("read");
                        // Error handling
                    }
                } else if (count == 0) {
                    // End of file: the client closed the connection
                    close(fd); // Close the socket
                    continue; // Next event
                }

                // Echo the received data back to the client
                ssize_t write_result = write(fd, buffer, count);
                if (write_result == -1) {
                    if (errno != EAGAIN) {
                        perror("write");
                        // Error handling
                    }
                    close(fd); // Handle failed write by closing the connection
                }

                // Normally you should handle possible partial write by continuing
                // to write the remaining bytes after an EAGAIN,
                // this would best be handled by a stateful mechanism for each connection
            }
        }
    }

    // Dispose of the epoll instance before program exit (not shown in this event loop)
    close(epollfd);

    return 0;
}
