#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#define SERVER_PORT 5000
#define SERVER_BACKLOG 5

#define MAX_CLIENTS 20

#define BUFF_SIZE 1024

int find_sock(int *conn_socks, int conn_count, int searching_sock);
void remove_and_shift_arr(int *conn_socks, int *conn_count, int searching_sock);
void close_conn_socks(int *conn_socks, int conn_count);

int main(int argc, char **argv) {
    int listen_sock;

    if ((listen_sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(SERVER_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };

    if (bind(listen_sock, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind");
        close(listen_sock);
        return 1;
    }

    if (listen(listen_sock, SERVER_BACKLOG) == -1) {
        perror("listen");
        close(listen_sock);
        return 1;
    }

    int epfd;

    if ((epfd = epoll_create1(0)) == -1) {
        perror("epoll_create1");
        close(listen_sock);
        return 1;
    }

    struct epoll_event event, events[MAX_CLIENTS];

    event.events = EPOLLIN;
    event.data.fd = listen_sock;

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, listen_sock, &event) == -1) {
        perror("epoll_ctl");
        close(epfd);
        close(listen_sock);
        return 1;
    }

    int conn_socks[MAX_CLIENTS];
    int conn_count = 0;
    int nfds;

    char buff[BUFF_SIZE];
    int len;

    while (1) {
        if ((nfds = epoll_wait(epfd, events, MAX_CLIENTS, -1)) == -1) {
            perror("epoll_wait");
            close(epfd);
            close(listen_sock);
            return 1;
        }

        for (int i = 0; i < nfds; i++) {
            if (events[i].data.fd == listen_sock) {
                int temp_sock = accept(listen_sock, NULL, NULL);

                if (temp_sock == -1) {
                    perror("accept");
                    close(epfd);
                    close_conn_socks(conn_socks, conn_count);
                    close(listen_sock);
                    return 1;
                }

                if (conn_count < MAX_CLIENTS) {
                    conn_socks[conn_count++] = temp_sock;

                    event.events = EPOLLIN | EPOLLRDHUP;
                    event.data.fd = temp_sock;

                    if (epoll_ctl(epfd, EPOLL_CTL_ADD, temp_sock, &event) ==
                        -1) {
                        perror("epoll_ctl");
                        close(epfd);
                        close_conn_socks(conn_socks, conn_count);
                        close(listen_sock);
                        return 1;
                    }
                } else {
                    close(temp_sock);
                }
            } else {
                if (events[i].events & EPOLLRDHUP) {
                    epoll_ctl(epfd, EPOLL_CTL_DEL, events[i].data.fd, NULL);
                    remove_and_shift_arr(conn_socks, &conn_count,
                                         events[i].data.fd);
                } else if (events[i].events & EPOLLIN) {
                    if ((len = recv(events[i].data.fd, buff, BUFF_SIZE - 1,
                                    0)) == -1) {
                        perror("recv");
                        close(epfd);
                        close_conn_socks(conn_socks, conn_count);
                        close(listen_sock);
                        return 1;
                    }

                    buff[len] = '\0';

                    for (int j = 0; j < conn_count; j++) {
                        if (conn_socks[j] == events[i].data.fd)
                            continue;

                        if (send(conn_socks[j], buff, len, 0) == -1) {
                            perror("send");
                            close(epfd);
                            close_conn_socks(conn_socks, conn_count);
                            close(listen_sock);
                            return 1;
                        }
                    }

                    printf("Received: \"%s\"\n", buff);

                    strcpy(buff, "OK");
                    if (send(events[i].data.fd, buff, strlen(buff), 0) == -1) {
                        perror("send");
                        close(epfd);
                        close_conn_socks(conn_socks, conn_count);
                        close(listen_sock);
                        return 1;
                    }
                }
            }
        }
    }

    close(epfd);
    close_conn_socks(conn_socks, conn_count);
    close(listen_sock);

    return 0;
}

int find_sock(int *conn_socks, int conn_count, int searching_sock) {
    for (int i = 0; i < conn_count; i++) {
        if (conn_socks[i] == searching_sock)
            return i;
    }

    return -1;
}

void remove_and_shift_arr(int *conn_socks, int *conn_count,
                          int searching_sock) {
    int idx = find_sock(conn_socks, *conn_count, searching_sock);

    if (idx >= 0) {
        memmove(conn_socks + idx, conn_socks + idx + 1,
                sizeof(int) * (*conn_count - idx - 1));
        (*conn_count)--;
    }
}

void close_conn_socks(int *conn_socks, int conn_count) {
    for (int i = 0; i < conn_count; i++)
        close(conn_socks[i]);
}
