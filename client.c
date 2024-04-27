#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define SERVER_PORT 5000

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

    char buff[BUFF_SIZE];
    int len;

    while (1) {
        fgets(buff, BUFF_SIZE, stdin);

        len = strlen(buff);
        buff[--len] = '\0'; // remove newline

        if (!len)
            continue;

        if (send(sockfd, buff, len, 0) == -1) {
            perror("send");
            close(sockfd);
            return 1;
        }

        if ((len = recv(sockfd, buff, BUFF_SIZE - 1, 0)) == -1) {
            perror("recv");
            close(sockfd);
            return 1;
        }

        buff[len] = '\0';
        printf("Received: \"%s\"\n\n", buff);
    }

    close(sockfd);
    return 0;
}
