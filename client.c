#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>

#define SERVER_IP "127.0.0.1"
#define PORT 9090
#define BUFFER_SIZE 1024

int main() {
    int sockfd;
    struct sockaddr_in server_addr;
    socklen_t server_len = sizeof(server_addr);
    char buffer[BUFFER_SIZE];
    ssize_t n;
    time_t now;
    struct tm *tm_info;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Initial message so server learns client address
    const char *start = "START\n";
    if (sendto(sockfd, start, strlen(start), 0,
               (struct sockaddr *)&server_addr, server_len) < 0) {
        perror("sendto START");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Receive "Bonjour"
    n = recvfrom(sockfd, buffer, BUFFER_SIZE - 1, 0,
                 (struct sockaddr *)&server_addr, &server_len);
    if (n < 0) {
        perror("recvfrom Bonjour");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    buffer[n] = '\0';
    printf("%s", buffer);

    // Send current time 60 times
    for (int i = 0; i < 60; i++) {
        now = time(NULL);
        tm_info = localtime(&now);

        snprintf(buffer, sizeof(buffer),
                 "Il est %02d:%02d:%02d !\n",
                 tm_info->tm_hour,
                 tm_info->tm_min,
                 tm_info->tm_sec);

        if (sendto(sockfd, buffer, strlen(buffer), 0,
                   (struct sockaddr *)&server_addr, server_len) < 0) {
            perror("sendto time");
            close(sockfd);
            exit(EXIT_FAILURE);
        }

        sleep(1);
    }

    // Receive "Au revoir"
    n = recvfrom(sockfd, buffer, BUFFER_SIZE - 1, 0,
                 (struct sockaddr *)&server_addr, &server_len);
    if (n < 0) {
        perror("recvfrom Au revoir");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    buffer[n] = '\0';
    printf("%s", buffer);

    close(sockfd);
    return 0;
}