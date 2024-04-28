#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#define SERVER_PORT 5000

#define MAX_EVENTS 2

#define BUFF_SIZE 1024

int main(void) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(SERVER_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };

    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("connect");
        close(sockfd);
        return 1;
    }

    int epfd;

    if ((epfd = epoll_create1(0)) == -1) {
        perror("epoll_create1");
        close(sockfd);
        return 1;
    }

    struct epoll_event event, events[MAX_EVENTS];

    event.events = EPOLLIN;
    event.data.fd = STDIN_FILENO;

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, STDIN_FILENO, &event) == -1) {
        perror("epoll_ctl");
        close(epfd);
        close(sockfd);
        return 1;
    }

    event.events = EPOLLIN | EPOLLRDHUP;
    event.data.fd = sockfd;

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &event) == -1) {
        perror("epoll_ctl");
        close(epfd);
        close(sockfd);
        return 1;
    }

    char buff[BUFF_SIZE];
    int len;
    int nfds;

    while (1) {
        if ((nfds = epoll_wait(epfd, events, MAX_EVENTS, -1)) == -1) {
            perror("epoll_wait");
            close(epfd);
            close(sockfd);
            return 1;
        }

        for (int i = 0; i < nfds; i++) {
            if (events[i].data.fd == STDIN_FILENO) {
                fgets(buff, BUFF_SIZE, stdin);

                len = strlen(buff);
                buff[--len] = '\0'; // remove newline

                if (!len)
                    continue;

                if (send(sockfd, buff, len, 0) == -1) {
                    close(epfd);
                    perror("send");
                    close(sockfd);
                    return 1;
                }
            } else if (events[i].data.fd == sockfd) {
                if (events[i].events & EPOLLRDHUP) {
                    close(epfd);
                    close(sockfd);
                    return 0;
                }

                if (events[i].events & EPOLLIN) {
                    if ((len = recv(sockfd, buff, BUFF_SIZE - 1, 0)) == -1) {
                        close(epfd);
                        perror("recv");
                        close(sockfd);
                        return 1;
                    }

                    buff[len] = '\0';
                    printf("Received: \"%s\"\n\n", buff);
                }
            }
        }
    }

    close(epfd);
    close(sockfd);
    return 0;
}
