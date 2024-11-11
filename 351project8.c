#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>

#define DEFAULT_PORT 80
#define BUFFER_SIZE 1024

int request_count = 0;
int received_bytes = 0;
int sent_bytes = 0;
pthread_mutex_t stats_lock;

void *handle_client(void *arg) {
    int client_sock = *(int *)arg;
    free(arg);

    char buffer[BUFFER_SIZE];
    int bytes = read(client_sock, buffer, BUFFER_SIZE);

    pthread_mutex_lock(&stats_lock);
    request_count++;
    received_bytes += bytes;
    pthread_mutex_unlock(&stats_lock);

    char method[16];
    char path[BUFFER_SIZE];
    sscanf(buffer, "%s %s", method, path);

    if (strcmp(method, "GET") != 0) {
        char response[] = "HTTP/1.1 405 Method Not Allowed\r\n\r\n";
        write(client_sock, response, sizeof(response) - 1);
    } 
    else if (strncmp(path, "/static/", 8) == 0) {
        char filepath[BUFFER_SIZE] = "./static";
        strncat(filepath, path + 7, BUFFER_SIZE - strlen(filepath) - 1);

        int file = open(filepath, O_RDONLY);
        if (file < 0) {
            char response[] = "HTTP/1.1 404 Not Found\r\n\r\nFile Not Found";
            write(client_sock, response, sizeof(response) - 1);
        } 
        else {
            struct stat st;
            fstat(file, &st);
            int file_size = st.st_size;

            char headers[BUFFER_SIZE];
            sprintf(headers, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\nContent-Type: application/octet-stream\r\n\r\n", file_size);
            write(client_sock, headers, strlen(headers));

            char file_buffer[BUFFER_SIZE];
            int file_bytes;
            while ((file_bytes = read(file, file_buffer, BUFFER_SIZE)) > 0) {
                write(client_sock, file_buffer, file_bytes);
                pthread_mutex_lock(&stats_lock);
                sent_bytes += file_bytes;
                pthread_mutex_unlock(&stats_lock);
            }
            close(file);
        }
    } 
    else if (strcmp(path, "/stats") == 0) {
        char stats_response[BUFFER_SIZE];
        pthread_mutex_lock(&stats_lock);
        sprintf(stats_response, "<html><body><h1>Server Stats</h1><p>Requests: %d</p><p>Received Bytes: %d</p><p>Sent Bytes: %d</p></body></html>", request_count, received_bytes, sent_bytes);
        pthread_mutex_unlock(&stats_lock);

        char headers[BUFFER_SIZE];
        sprintf(headers, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\nContent-Type: text/html\r\n\r\n", (int)strlen(stats_response));
        write(client_sock, headers, strlen(headers));
        write(client_sock, stats_response, strlen(stats_response));

        pthread_mutex_lock(&stats_lock);
        sent_bytes += strlen(headers) + strlen(stats_response);
        pthread_mutex_unlock(&stats_lock);
    } 
    else if (strncmp(path, "/calc?", 6) == 0) {
        int a = 0, b = 0;
        sscanf(path, "/calc?a=%d&b=%d", &a, &b);
        int sum = a + b;

        char calc_response[BUFFER_SIZE];
        sprintf(calc_response, "<html><body><p>Result of %d + %d = %d</p></body></html>", a, b, sum);

        char headers[BUFFER_SIZE];
        sprintf(headers, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\nContent-Type: text/html\r\n\r\n", (int)strlen(calc_response));
        write(client_sock, headers, strlen(headers));
        write(client_sock, calc_response, strlen(calc_response));

        pthread_mutex_lock(&stats_lock);
        sent_bytes += strlen(headers) + strlen(calc_response);
        pthread_mutex_unlock(&stats_lock);
    } 
    else {
        char response[] = "HTTP/1.1 404 Not Found\r\n\r\nNot Found";
        write(client_sock, response, sizeof(response) - 1);
    }
    close(client_sock);
    return NULL;
}

int main(int argc, char *argv[]) {
    int port = DEFAULT_PORT;
    if (argc > 2 && strcmp(argv[1], "-p") == 0) {
        port = atoi(argv[2]);
    }

    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
    listen(server_sock, 10);

    pthread_mutex_init(&stats_lock, NULL);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int *client_sock = malloc(sizeof(int));
        *client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_len);

        pthread_t thread;
        pthread_create(&thread, NULL, handle_client, client_sock);
        pthread_detach(thread);
    }

    pthread_mutex_destroy(&stats_lock);
    close(server_sock);
    return 0;
}
