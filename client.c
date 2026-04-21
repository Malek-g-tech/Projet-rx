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
    int sock;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    ssize_t n;
    time_t now;
    struct tm *tm_info;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(sock);
        exit(EXIT_FAILURE);
    }

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Receive "Bonjour" from station 1
    n = recv(sock, buffer, BUFFER_SIZE - 1, 0);
    if (n < 0) {
        perror("recv Bonjour");
        close(sock);
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

        if (send(sock, buffer, strlen(buffer), 0) < 0) {
            perror("send time");
            close(sock);
            exit(EXIT_FAILURE);
        }

        sleep(1);
    }

    // Receive "Au revoir" from station 1
    n = recv(sock, buffer, BUFFER_SIZE - 1, 0);
    if (n < 0) {
        perror("recv Au revoir");
        close(sock);
        exit(EXIT_FAILURE);
    }
    buffer[n] = '\0';
    printf("%s", buffer);

    close(sock);
    return 0;
}