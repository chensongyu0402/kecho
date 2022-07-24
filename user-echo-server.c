#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/epoll.h>
#include <time.h>
#include <unistd.h>

#include "user-echo-server.h"

#define SERVER_PORT 12345
#define EPOLL_SIZE 256
#define BUF_SIZE 512
#define EPOLL_RUN_TIMEOUT -1

struct runtime_statistics stats = {0};

static int setnonblock(int fd)
{
    int fdflags;
    if ((fdflags = fcntl(fd, F_GETFL, 0)) == -1)
        return -1;
    fdflags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, fdflags) == -1)
        return -1;
    return 0;
}

static int handle_message_from_client(int client, client_list_t **list)
{
    int len;
    char buf[BUF_SIZE];
    memset(buf, 0, BUF_SIZE);
    if ((len = recv(client, buf, BUF_SIZE, 0)) < 0)
        server_err("Fail to receive", list);
    if (len == 0) {
        if (close(client) < 0)
            server_err("Fail to close", list);
        *list = delete_client(list, client);
        TRACE(shdn_msg);
    } else {
        TRACE(recv_msg);
        if (send(client, buf, BUF_SIZE, 0) < 0)
            server_err("Fail to send", list);
        TRACE(send_msg);
    }
    return len;
}

int main(void)
{
    RUNTIME_STAT_INIT();
    static struct epoll_event events[EPOLL_SIZE];
    struct sockaddr_in addr = {
        .sin_family = PF_INET,
        .sin_port = htons(SERVER_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    socklen_t socklen = sizeof(addr);

    client_list_t *list = NULL;
    int listener;
    if ((listener = socket(PF_INET, SOCK_STREAM, 0)) < 0)
        server_err("Fail to create socket", &list);
    printf("Main listener (fd=%d) was created.\n", listener);

    if (setnonblock(listener) == -1)
        server_err("Fail to set nonblocking", &list);
    if (bind(listener, (struct sockaddr *) &addr, sizeof(addr)) < 0)
        server_err("Fail to bind", &list);
    printf("Listener was binded to %s\n", inet_ntoa(addr.sin_addr));

    /*
     * the backlog, which is "128" here, is also the default attribute of
     * "/proc/sys/net/core/somaxconn" before Linux 5.4.
     *
     * specifying the backlog greater than "somaxconn" will be truncated
     * to the maximum attribute of "somaxconn" silently. But you can also
     * adjust "somaxconn" by using command:
     *
     * $ sudo sysctl net.core.somaxconn=<value>
     *
     * For details, please refer to:
     *
     * http://man7.org/linux/man-pages/man2/listen.2.html
     */
    if (listen(listener, 128) < 0)
        server_err("Fail to listen", &list);

    int epoll_fd;
    if ((epoll_fd = epoll_create(EPOLL_SIZE)) < 0)
        server_err("Fail to create epoll", &list);

    static struct epoll_event ev = {.events = EPOLLIN | EPOLLET};
    ev.data.fd = listener;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listener, &ev) < 0)
        server_err("Fail to control epoll", &list);
    printf("Listener (fd=%d) was added to epoll.\n", epoll_fd);

    while (1) {
        struct sockaddr_in client_addr;
        int epoll_events_count;
        if ((epoll_events_count = epoll_wait(epoll_fd, events, EPOLL_SIZE,
                                             EPOLL_RUN_TIMEOUT)) < 0)
            server_err("Fail to wait epoll", &list);
        stats.epll_cnt = epoll_events_count;

        for (int i = 0; i < epoll_events_count; i++) {
            /* EPOLLIN event for listener (new client connection) */
            if (events[i].data.fd == listener) {
                int client;
                while (
                    (client = accept(listener, (struct sockaddr *) &client_addr,
                                     &socklen)) > 0) {
                    setnonblock(client);
                    ev.data.fd = client;
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client, &ev) < 0)
                        server_err("Fail to control epoll", &list);
                    push_back_client(&list, client,
                                     inet_ntoa(client_addr.sin_addr));
                   TRACE(clnt_cnt);
                }
                if (errno != EWOULDBLOCK)
                    server_err("Fail to accept", &list);
            } else {
                /* EPOLLIN event for others (new incoming message from client)
                 */
                if (handle_message_from_client(events[i].data.fd, &list) < 0)
                    server_err("Handle message from client", &list);
            }
        }
    }
    close(listener);
    close(epoll_fd);
    exit(0);
}
