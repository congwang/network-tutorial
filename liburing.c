// Reference: https://unixism.net/loti/tutorial/webserver_liburing.html

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <liburing.h>
#include <arpa/inet.h>

#define LISTEN_PORT 8080
#define BACKLOG 32
#define READ_SZ 8192

#define EVENT_TYPE_ACCEPT       0
#define EVENT_TYPE_READ         1
#define EVENT_TYPE_WRITE        2

struct io_uring ring;

struct request {
    int event_type;
    int client_socket;
    struct iovec iov[];
};

int add_accept_request(int server_socket, struct sockaddr_in *client_addr, socklen_t *client_addr_len) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    struct request *req = malloc(sizeof(struct request));

    req->event_type = EVENT_TYPE_ACCEPT;

    io_uring_prep_accept(sqe, server_socket, (struct sockaddr *) client_addr, client_addr_len, 0);
    io_uring_sqe_set_data(sqe, req);
    io_uring_submit(&ring);
    return 0;
}

int add_read_request(int client_socket) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    struct request *req = malloc(sizeof(*req) + sizeof(struct iovec));

    req->event_type = EVENT_TYPE_READ;
    req->iov[0].iov_base = malloc(READ_SZ);
    req->iov[0].iov_len = READ_SZ;
    req->client_socket = client_socket;
    memset(req->iov[0].iov_base, 0, READ_SZ);

    /* Linux kernel 5.5 has support for readv, but not for recv() or read() */
    io_uring_prep_readv(sqe, client_socket, &req->iov[0], 1, 0);
    io_uring_sqe_set_data(sqe, req);
    io_uring_submit(&ring);
    return 0;
}

int add_write_request(struct request *req) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);

    req->event_type = EVENT_TYPE_WRITE;

    io_uring_prep_writev(sqe, req->client_socket, req->iov, 1, 0);
    io_uring_sqe_set_data(sqe, req);
    io_uring_submit(&ring);
    return 0;
}

void handle_client_request(struct request *req, int length) {
    struct request *new_req = malloc(sizeof(*req) + sizeof(struct iovec));

    new_req->iov[0].iov_base = malloc(length);
    new_req->iov[0].iov_len = length;
    new_req->client_socket = req->client_socket;
    memcpy(new_req->iov[0].iov_base, req->iov[0].iov_base, length);
    add_write_request(new_req);
}

int main(int argc, char *argv[]) {
    struct sockaddr_in serv_addr;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int server_fd, ret;

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Set up server address
    memset((char *)&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(LISTEN_PORT);

    // Bind to the port
    if (bind(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))<0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    
    // Start listening on the socket
    if (listen(server_fd, BACKLOG) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Listening on port %d\n", LISTEN_PORT);

    // Initialize io_uring
    ret = io_uring_queue_init(256, &ring, 0);
    if (ret < 0) {
        perror("io_uring_queue_init");
        exit(EXIT_FAILURE);
    }

    struct io_uring_cqe *cqe;

    // Prepare an accept SQE
    add_accept_request(server_fd, &client_addr, &client_addr_len);

    while (1) {
        // Wait for completion
        ret = io_uring_wait_cqe(&ring, &cqe);
        if (ret < 0) {
            perror("io_uring_wait_cqe");
            exit(1);
        }

        if (cqe->res < 0) {
            fprintf(stderr, "Async operation failed: %s\n", strerror(-cqe->res));
            exit(1);
        }

        struct request *req = (struct request *)cqe->user_data;

        switch (req->event_type) {
            case EVENT_TYPE_ACCEPT:
                printf("Accepted connection from %s\n", inet_ntoa(client_addr.sin_addr));
                add_accept_request(server_fd, &client_addr, &client_addr_len);
                add_read_request(cqe->res);
                free(req);
                break;
            case EVENT_TYPE_READ:
                if (!cqe->res) {
                    fprintf(stderr, "Empty request!\n");
                    break;
                }
                handle_client_request(req, cqe->res);
                free(req->iov[0].iov_base);
                free(req);
                break;
            case EVENT_TYPE_WRITE:
                free(req->iov[0].iov_base);
                close(req->client_socket);
                free(req);
                break;
        }
        io_uring_cqe_seen(&ring, cqe);
    }

    close(server_fd);
    io_uring_queue_exit(&ring);

    return 0;
}
