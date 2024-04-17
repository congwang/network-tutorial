#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <event.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define LISTEN_PORT 8080
#define BUFFER_SIZE 1024

void echo_cb(int fd, short events, void *arg) {
    struct event *client_ev = arg;
    char buf[BUFFER_SIZE];
    int len = read(fd, buf, sizeof(buf) - 1);
    
    if (len > 0) {
        buf[len] = '\0';
        write(fd, buf, len); // Echo the data back
    } else {
        // Connection closed by client or an error occurred
        if (len == 0) {
            printf("Client disconnected\n");
        } else {
            perror("read");
        }
        close(fd);
        event_del(client_ev);  // Remove the event from the event_base
        free(client_ev);  // Free the event
    }
}

void accept_cb(int fd, short events, void *arg) {
    struct event *ev = arg;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(fd, (struct sockaddr *)&client_addr, &client_len);

    if (client_fd < 0) {
        perror("accept");
        return;
    }

    printf("Accepted connection from %s\n", inet_ntoa(client_addr.sin_addr));

    // Create a new event for the accepted client for reading
    struct event *client_ev = malloc(sizeof(struct event));
    event_set(client_ev, client_fd, EV_READ | EV_PERSIST, echo_cb, client_ev);
    event_base_set(ev->ev_base, client_ev);
    event_add(client_ev, NULL);
}

int main() {
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        return 1;
    }

    int reuseaddr_on = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_on, sizeof(reuseaddr_on));

    struct sockaddr_in listen_addr;
    memset(&listen_addr, 0, sizeof(listen_addr));
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_addr.s_addr = INADDR_ANY;
    listen_addr.sin_port = htons(LISTEN_PORT);

    if (bind(listen_fd, (struct sockaddr *)&listen_addr, sizeof(listen_addr)) < 0) {
        perror("bind");
        return 1;
    }

    if (listen(listen_fd, 10) < 0) {
        perror("listen");
        return 1;
    }

    printf("Listening on port %d\n", LISTEN_PORT);

    // Initialize the event API
    struct event_base *base = event_base_new();
    struct event listen_ev;
    
    // Set up an event for the listening socket
    event_set(&listen_ev, listen_fd, EV_READ | EV_PERSIST, accept_cb, &listen_ev);
    event_base_set(base, &listen_ev);
    event_add(&listen_ev, NULL);

    // Start the event loop
    event_base_dispatch(base);

    event_base_free(base);
    close(listen_fd);
    return 0;
}