#define _GNU_SOURCE

#include <arpa/inet.h>
#include <ctype.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080
#define BACKLOG 10

#define REQ_BUFFER_SIZE 16384
#define MAX_METHOD 16
#define MAX_PATH 1024
#define MAX_VERSION 16
#define MAX_HEADERS 100

typedef struct {
    char name[256];
    char value[1024];
} Header;

typedef struct {
    char method[MAX_METHOD];
    char path[MAX_PATH];
    char version[MAX_VERSION];

    Header headers[MAX_HEADERS];
    int header_count;

    const char *body;
    size_t body_length;
} HttpRequest;

static void trim_spaces(char *s) {
    char *start = s;
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }

    if (start != s) {
        memmove(s, start, strlen(start) + 1);
    }

    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[len - 1] = '\0';
        len--;
    }
}

static const char *get_header_value(const HttpRequest *req, const char *header_name) {
    for (int i = 0; i < req->header_count; i++) {
        if (strcasecmp(req->headers[i].name, header_name) == 0) {
            return req->headers[i].value;
        }
    }
    return NULL;
}

static long read_content_length_from_headers(const char *raw, size_t headers_len) {
    char headers_copy[REQ_BUFFER_SIZE];
    if (headers_len + 1 > sizeof(headers_copy)) {
        return -1;
    }

    memcpy(headers_copy, raw, headers_len);
    headers_copy[headers_len] = '\0';

    char *saveptr = NULL;
    char *line = strtok_r(headers_copy, "\r\n", &saveptr);
    while (line != NULL) {
        if (strncasecmp(line, "Content-Length:", 15) == 0) {
            char *value = line + 15;
            while (*value && isspace((unsigned char)*value)) {
                value++;
            }

            if (*value == '\0') {
                return -1;
            }

            char *endptr = NULL;
            long parsed = strtol(value, &endptr, 10);
            if (*endptr != '\0' || parsed < 0) {
                return -1;
            }
            return parsed;
        }

        line = strtok_r(NULL, "\r\n", &saveptr);
    }

    return 0;
}

static ssize_t read_http_request(int client_fd, char *buffer, size_t buffer_size) {
    size_t total = 0;
    ssize_t header_end_index = -1;
    long expected_body_len = 0;

    while (total < buffer_size - 1) {
        ssize_t n = recv(client_fd, buffer + total, buffer_size - 1 - total, 0);
        if (n < 0) {
            perror("recv");
            return -1;
        }
        if (n == 0) {
            break;
        }

        total += (size_t)n;
        buffer[total] = '\0';

        if (header_end_index < 0) {
            char *headers_end = strstr(buffer, "\r\n\r\n");
            if (headers_end != NULL) {
                header_end_index = (ssize_t)(headers_end - buffer);
                expected_body_len =
                    read_content_length_from_headers(buffer, (size_t)header_end_index + 4);
                if (expected_body_len < 0) {
                    return -1;
                }
            }
        }

        if (header_end_index >= 0) {
            size_t body_received = total - ((size_t)header_end_index + 4);
            if (body_received >= (size_t)expected_body_len) {
                break;
            }
        }
    }

    if (total == 0) {
        return 0;
    }

    if (header_end_index < 0) {
        return -1;
    }

    return (ssize_t)total;
}

static int parse_http_request(char *raw, size_t raw_len, HttpRequest *req) {
    memset(req, 0, sizeof(*req));

    char *headers_end = strstr(raw, "\r\n\r\n");
    if (headers_end == NULL) {
        fprintf(stderr, "Incomplete HTTP request: end of headers not found\n");
        return -1;
    }

    req->body = headers_end + 4;
    req->body_length = raw_len - (size_t)(req->body - raw);

    *headers_end = '\0';

    char *line_end = strstr(raw, "\r\n");
    if (line_end == NULL) {
        fprintf(stderr, "Invalid HTTP request: missing request line ending\n");
        return -1;
    }

    *line_end = '\0';

    if (sscanf(raw, "%15s %1023s %15s", req->method, req->path, req->version) != 3) {
        fprintf(stderr, "Invalid request line\n");
        return -1;
    }

    char *line = line_end + 2;
    while (*line != '\0') {
        char *next = strstr(line, "\r\n");
        if (next != NULL) {
            *next = '\0';
        }

        if (*line == '\0') {
            break;
        }

        char *colon = strchr(line, ':');
        if (colon == NULL) {
            fprintf(stderr, "Malformed header: %s\n", line);
            return -1;
        }

        if (req->header_count >= MAX_HEADERS) {
            fprintf(stderr, "Too many headers\n");
            return -1;
        }

        *colon = '\0';

        strncpy(req->headers[req->header_count].name, line,
                sizeof(req->headers[req->header_count].name) - 1);
        strncpy(req->headers[req->header_count].value, colon + 1,
                sizeof(req->headers[req->header_count].value) - 1);

        trim_spaces(req->headers[req->header_count].name);
        trim_spaces(req->headers[req->header_count].value);

        req->header_count++;

        if (next == NULL) {
            break;
        }
        line = next + 2;
    }

    const char *content_length_value = get_header_value(req, "Content-Length");
    if (content_length_value != NULL) {
        char *endptr = NULL;
        long expected = strtol(content_length_value, &endptr, 10);
        if (*content_length_value == '\0' || (endptr != NULL && *endptr != '\0') || expected < 0) {
            fprintf(stderr, "Invalid Content-Length value\n");
            return -1;
        }
        if ((size_t)expected != req->body_length) {
            fprintf(stderr, "Body length mismatch: expected %ld, got %zu\n", expected, req->body_length);
            return -1;
        }
    }

    return 0;
}

static int send_http_response(int client_fd, const char *status, const char *content_type,
                              const char *body) {
    char response[8192];
    int body_length = (int)strlen(body);

    int len = snprintf(response, sizeof(response),
                       "HTTP/1.1 %s\r\n"
                       "Content-Type: %s\r\n"
                       "Content-Length: %d\r\n"
                       "Connection: close\r\n"
                       "\r\n"
                       "%s",
                       status, content_type, body_length, body);

    if (len < 0 || len >= (int)sizeof(response)) {
        fprintf(stderr, "Response too large\n");
        return -1;
    }

    ssize_t total_sent = 0;
    while (total_sent < len) {
        ssize_t sent = send(client_fd, response + total_sent, (size_t)(len - total_sent), 0);
        if (sent <= 0) {
            perror("send");
            return -1;
        }
        total_sent += sent;
    }

    return 0;
}

static int send_http_response_head_only(int client_fd, const char *status, const char *content_type,
                                        const char *body) {
    char response[8192];
    int body_length = (int)strlen(body);

    int len = snprintf(response, sizeof(response),
                       "HTTP/1.1 %s\r\n"
                       "Content-Type: %s\r\n"
                       "Content-Length: %d\r\n"
                       "Connection: close\r\n"
                       "\r\n",
                       status, content_type, body_length);

    if (len < 0 || len >= (int)sizeof(response)) {
        fprintf(stderr, "Response too large\n");
        return -1;
    }

    ssize_t total_sent = 0;
    while (total_sent < len) {
        ssize_t sent = send(client_fd, response + total_sent, (size_t)(len - total_sent), 0);
        if (sent <= 0) {
            perror("send");
            return -1;
        }
        total_sent += sent;
    }

    return 0;
}

static int handle_http_request(int client_fd, const HttpRequest *req) {
    if (strcmp(req->method, "HEAD") == 0) {
        if (strcmp(req->path, "/") == 0) {
            return send_http_response_head_only(client_fd, "200 OK", "text/plain",
                                                "Welcome to my HTTP server");
        }

        if (strcmp(req->path, "/hello") == 0) {
            return send_http_response_head_only(client_fd, "200 OK", "text/plain",
                                                "Hello from C HTTP server");
        }

        return send_http_response_head_only(client_fd, "404 Not Found", "text/plain",
                                            "404 page not found");
    }

    if (strcmp(req->method, "GET") == 0) {
        if (strcmp(req->path, "/") == 0) {
            return send_http_response(client_fd, "200 OK", "text/plain",
                                      "Welcome to my HTTP server");
        }

        if (strcmp(req->path, "/hello") == 0) {
            return send_http_response(client_fd, "200 OK", "text/plain",
                                      "Hello from C HTTP server");
        }
    }

    if (strcmp(req->method, "POST") == 0 && strcmp(req->path, "/echo") == 0) {
        char body_msg[2048];
        int n = snprintf(body_msg, sizeof(body_msg), "Received body: %.*s", (int)req->body_length,
                         req->body);
        if (n < 0 || n >= (int)sizeof(body_msg)) {
            return send_http_response(client_fd, "413 Payload Too Large", "text/plain",
                                      "Request body too large");
        }
        return send_http_response(client_fd, "200 OK", "text/plain", body_msg);
    }

    return send_http_response(client_fd, "404 Not Found", "text/plain", "404 page not found");
}

int main(void) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt SO_REUSEADDR");
        close(server_fd);
        return 1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_IP, &addr.sin_addr) != 1) {
        perror("inet_pton");
        close(server_fd);
        return 1;
    }

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, BACKLOG) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    signal(SIGCHLD, SIG_IGN);

    printf("Server listening on %s:%d\n", SERVER_IP, SERVER_PORT);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            close(client_fd);
            continue;
        }

        if (pid == 0) {
            close(server_fd);

            char request_buffer[REQ_BUFFER_SIZE];
            ssize_t bytes_read =
                read_http_request(client_fd, request_buffer, sizeof(request_buffer));
            if (bytes_read <= 0) {
                send_http_response(client_fd, "400 Bad Request", "text/plain",
                                   "Invalid HTTP request");
                close(client_fd);
                _exit(0);
            }

            request_buffer[bytes_read] = '\0';

            HttpRequest req;
            if (parse_http_request(request_buffer, (size_t)bytes_read, &req) != 0) {
                send_http_response(client_fd, "400 Bad Request", "text/plain",
                                   "Invalid HTTP request");
                close(client_fd);
                _exit(0);
            }

            handle_http_request(client_fd, &req);
            close(client_fd);
            _exit(0);
        }

        close(client_fd);
    }

    close(server_fd);
    return 0;
}
