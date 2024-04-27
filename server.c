#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#define SERVER_PORT 5000
#define SERVER_BACKLOG 5

#define MAX_CLIENTS 50

#define BUFFER_SIZE 1024

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

    int nfds;

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
                    close(listen_sock);
                    return 1;
                }

                event.events = EPOLLIN | EPOLLRDHUP;
                event.data.fd = temp_sock;

                if (epoll_ctl(epfd, EPOLL_CTL_ADD, temp_sock, &event) == -1) {
                    perror("epoll_ctl");
                    close(epfd);
                    close(listen_sock);
                    return 1;
                }
            } else {
                if (events[i].events & EPOLLRDHUP) {
                    epoll_ctl(epfd, EPOLL_CTL_DEL, events[i].data.fd, NULL);
                } else if (events[i].events & EPOLLIN) {
                    char buff[1024];
                    int len;

                    if ((len = recv(events[i].data.fd, buff, BUFFER_SIZE - 1,
                                    0)) == -1) {
                        perror("recv");
                        close(epfd);
                        close(listen_sock);
                        return 1;
                    }

                    buff[len] = '\0';

                    printf("Received: \"%s\"\n", buff);

                    strcpy(buff, "OK");
                    send(events[i].data.fd, buff, strlen(buff), 0);
                }
            }
        }
    }

    close(epfd);
    close(listen_sock);

    return 0;
}
