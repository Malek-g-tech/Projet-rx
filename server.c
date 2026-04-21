#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>

#define PORT 9090
#define BUFFER_SIZE 1024

int main() {
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];
    ssize_t n;
    int msg_count = 0;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    memset(&client_addr, 0, sizeof(client_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("UDP server listening on port %d...\n", PORT);

    // Wait for an initial message from client so we know its address
    n = recvfrom(sockfd, buffer, BUFFER_SIZE - 1, 0,
                 (struct sockaddr *)&client_addr, &client_len);
    if (n < 0) {
        perror("recvfrom");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    buffer[n] = '\0';
    printf("Client identified: %s:%d\n",
           inet_ntoa(client_addr.sin_addr),
           ntohs(client_addr.sin_port));
    printf("Initial message: %s", buffer);

    // Send "Bonjour"
    const char *hello = "Bonjour\n";
    if (sendto(sockfd, hello, strlen(hello), 0,
               (struct sockaddr *)&client_addr, client_len) < 0) {
        perror("sendto Bonjour");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Receive the remaining 60 time messages
    while (msg_count < 60) {
        n = recvfrom(sockfd, buffer, BUFFER_SIZE - 1, 0,
                     (struct sockaddr *)&client_addr, &client_len);
        if (n < 0) {
            perror("recvfrom");
            close(sockfd);
            exit(EXIT_FAILURE);
        }

        buffer[n] = '\0';
        printf("%s", buffer);
        msg_count++;
    }

    // Send "Au revoir"
    const char *bye = "Au revoir\n";
    if (sendto(sockfd, bye, strlen(bye), 0,
               (struct sockaddr *)&client_addr, client_len) < 0) {
        perror("sendto Au revoir");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("\nTotal time messages received: %d\n", msg_count);

    close(sockfd);
    return 0;
}