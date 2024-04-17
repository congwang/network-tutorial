#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/sock_diag.h>
#include <linux/inet_diag.h>
#include <linux/tcp.h>
#include <linux/errqueue.h>
#include <linux/if_packet.h>
#include <assert.h>

#define SERVER_PORT 8080
#define MSG "Hello, zero-copy world!\n"

#ifndef SO_EE_ORIGIN_ZEROCOPY
#define SO_EE_ORIGIN_ZEROCOPY		5
#endif

#ifndef SO_ZEROCOPY
#define SO_ZEROCOPY	60
#endif

#ifndef SO_EE_CODE_ZEROCOPY_COPIED
#define SO_EE_CODE_ZEROCOPY_COPIED	1
#endif

#ifndef MSG_ZEROCOPY
#define MSG_ZEROCOPY	0x4000000
#endif

static void do_recv_completion(int fd)
{
	struct sock_extended_err *serr;
	struct msghdr msg = {};
	struct cmsghdr *cm;
	uint32_t hi, lo;
	int ret, zerocopy;
	char control[100];

	msg.msg_control = control;
	msg.msg_controllen = sizeof(control);

	ret = recvmsg(fd, &msg, MSG_ERRQUEUE);
	if (ret == -1 && errno == EAGAIN)
		return;
	if (ret == -1)
		perror("recvmsg notification");
	if (msg.msg_flags & MSG_CTRUNC)
		perror("recvmsg notification: truncated");

	cm = CMSG_FIRSTHDR(&msg);
	if (!cm)
		perror("cmsg: no cmsg");
	if (!((cm->cmsg_level == SOL_IP && cm->cmsg_type == IP_RECVERR) ||
	      (cm->cmsg_level == SOL_IPV6 && cm->cmsg_type == IPV6_RECVERR) ||
	      (cm->cmsg_level == SOL_PACKET && cm->cmsg_type == PACKET_TX_TIMESTAMP)))
		perror("serr: wrong type");

	serr = (void *) CMSG_DATA(cm);

	if (serr->ee_origin != SO_EE_ORIGIN_ZEROCOPY)
		perror("serr: wrong origin");
	if (serr->ee_errno != 0)
		perror("serr: wrong error code");

	hi = serr->ee_data;
	lo = serr->ee_info;
	zerocopy = !(serr->ee_code & SO_EE_CODE_ZEROCOPY_COPIED);

    printf("Received Notification [%u-%u] zerocopy %d\n", hi, lo, zerocopy);
}

int main() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr;
    struct msghdr msg = {0};
    struct iovec iov = {0};
    int enable = 1;
    // Set the message to be sent
    char buffer[] = MSG;
    unsigned int len = strlen(buffer);

    // Create a server socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed");
        return EXIT_FAILURE;
    }

    // Allow quick reuse of the port
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        perror("setsockopt SO_REUSEADDR failed");
        return EXIT_FAILURE;
    }

    // Bind the server socket to a port
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        close(server_fd);
        return EXIT_FAILURE;
    }

    // Listen for client connections
    if (listen(server_fd, 1) < 0) {
        perror("listen failed");
        close(server_fd);
        return EXIT_FAILURE;
    }

    printf("Listening on port %d\n", SERVER_PORT);

    // Accept a client connection
    if ((client_fd = accept(server_fd, NULL, NULL)) < 0) {
        perror("accept failed");
        close(server_fd);
        return EXIT_FAILURE;
    }

    if (setsockopt(client_fd, SOL_SOCKET, SO_ZEROCOPY, &enable, sizeof(enable)))
		perror("setsockopt SOL_SOCKET.SO_ZEROCOPY");

    // Prepare the message header and I/O vector for zero-copy send
    iov.iov_base = buffer;
    iov.iov_len = len;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    // Send the message with MSG_ZEROCOPY flag
    if (sendmsg(client_fd, &msg, MSG_ZEROCOPY) != len) {
        perror("sendmsg failed");
        close(client_fd);
        close(server_fd);
        return EXIT_FAILURE;
    }

    // The application would likely perform further I/O operations here

    // Receive sendmsg notifications
    // Warning: localhost connection zerocopy will fallback to copy
    do_recv_completion(client_fd);

    // Cleanup
    close(client_fd);
    close(server_fd);

    return 0;
}