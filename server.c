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
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];
    ssize_t n;
    int msg_count = 0;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 5) < 0) {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", PORT);

    client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
    if (client_fd < 0) {
        perror("accept");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Client connected: %s:%d\n",
           inet_ntoa(client_addr.sin_addr),
           ntohs(client_addr.sin_port));

    // Send "Bonjour" immediately after connection
    const char *hello = "Bonjour\n";
    if (send(client_fd, hello, strlen(hello), 0) < 0) {
        perror("send Bonjour");
        close(client_fd);
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Receive 60 time messages from station 2
    while (msg_count < 60 && (n = recv(client_fd, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[n] = '\0';
        printf("%s", buffer);

        for (ssize_t i = 0; i < n; i++) {
            if (buffer[i] == '\n') {
                msg_count++;
            }
        }
    }

    if (n < 0) {
        perror("recv");
        close(client_fd);
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Send "Au revoir" after receiving the 60 messages
    const char *bye = "Au revoir\n";
    if (send(client_fd, bye, strlen(bye), 0) < 0) {
        perror("send Au revoir");
        close(client_fd);
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("\nTotal time messages received: %d\n", msg_count);

    close(client_fd);
    close(server_fd);
    return 0;
}