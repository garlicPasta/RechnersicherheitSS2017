/*
 *  Basic echo server was taken from here :
 *  https://github.com/mafintosh/echo-servers.c/blob/master/tcp-echo-server.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <time.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024
#define LOG_PATH "/var/log/ushoutd.log"
#define on_error(...) { fprintf(stderr, __VA_ARGS__); fflush(stderr); fclose(logFile); exit(1); }

static FILE *logFile;
FILE * create_log_file(void);

void log_to_file(char *message);
void get_date(char *buffer);

int main (int argc, char *argv[])
{
    logFile = create_log_file();
    if (argc < 2) on_error("Usage: %s [port]\n", argv[0]);
    int port = atoi(argv[1]);

    int server_fd, client_fd, err;
    struct sockaddr_in server, client;
    char buf[BUFFER_SIZE];

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) on_error("Could not create socket\n");

    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = htonl(INADDR_ANY);

    int opt_val = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof opt_val);

    err = bind(server_fd, (struct sockaddr *) &server, sizeof(server));
    if (err < 0) on_error("Could not bind socket\n");

    err = listen(server_fd, 128);
    if (err < 0) on_error("Could not listen on socket\n");

    printf("Server is listening on %d\n", port);

    char *log_connect = (char*)malloc(130 * sizeof(char));
    char *log_message = (char*)malloc((BUFFER_SIZE + 130) * sizeof(char));

    while (1) {
        socklen_t client_len = sizeof(client);
        client_fd = accept(server_fd, (struct sockaddr *) &client, &client_len);

        snprintf(
                log_connect,
                300,
                "User with ip address %s connected to the server",
                inet_ntoa(client.sin_addr)
        );

        log_to_file(log_connect);

        if (client_fd < 0) on_error("Could not establish new connection\n");

        while (1) {
            int read = recv(client_fd, buf, BUFFER_SIZE, 0);
            snprintf(
                    log_message,
                    BUFFER_SIZE + 100,
                    "%s send message: %s",
                    inet_ntoa(client.sin_addr),
                    buf
            );

            log_to_file(log_message);

            if (!read) break;
            if (read < 0) on_error("Client read failed\n");

            err = send(client_fd, buf, read, 0);
            if (err < 0) on_error("Client write failed\n");
        }
    }

    fclose(logFile);
    return 0;
}

void log_to_file(char *message)
{
    char date[26];
    get_date(date);
    fprintf(logFile, "%s: %s\n", date, message);
    fflush(logFile);
}

void get_date(char *buffer)
{
    time_t timer;
    struct tm* tm_info;

    time(&timer);
    tm_info = localtime(&timer);
    strftime(buffer, 26, "%Y-%m-%d %H:%M:%S", tm_info);
}

FILE * create_log_file() {
    FILE *f = fopen(LOG_PATH, "ab+");
    chmod(LOG_PATH, S_IRWXU);
    if (f == NULL)
    {
        printf("Error opening file!\n");
        exit(1);
    }
    return f;
}

