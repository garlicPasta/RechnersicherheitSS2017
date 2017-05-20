#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <time.h>
#include <arpa/inet.h>
#include <memory.h>
#include <pthread.h>

#define BUFFER_SIZE 1024
#define LOG_PATH "/var/log/ushoutd.log"
#define PASS_PATH "/etc/ushoutd.passwd"
#define on_error(...) { fprintf(stderr, __VA_ARGS__); fflush(stderr); fclose(logFile); exit(1); }

static FILE *logFile;
FILE * create_log_file(void);

typedef struct
{
    char *username;
    char *password;
} user;

typedef struct {
    int socket_addr;
    struct sockaddr_in client;
} thread_args;

void log_to_file(char *message);
void get_date(char *buffer);
void handle_request(thread_args);

user *createUser(char *username, char *password) {

    user *user = malloc(sizeof(user));

    user->username = strdup(username);
    user->password = strdup(password);
    return user;
}

static user *user_list[1];

void load_users(){
    FILE * fp;
    char * line = NULL;
    size_t len = 0;
    ssize_t read;

    fp = fopen(PASS_PATH, "r");
    if (fp == NULL)
        exit(EXIT_FAILURE);

    while ((read = getline(&line, &len, fp)) != -1) {
        char *token;
        char *user_data[2];
        int i = 0;
        while ((token = strsep(&line, ":"))) {
            user_data[i++] = token;
        }
        user *u = createUser(user_data[0], user_data[1]);
        user_list[0] = u;
    }

    fclose(fp);
    if (line)
        free(line);

}

int check_credentials(char *username, char *password) {

  for (int i = 0; i < sizeof(user_list); i++) {
    user *u = user_list[i];

    if (strcmp(username, u->username) && strcmp(password, u->password)) {
      return 1;
    }
  }

  return -1;
}

int main (int argc, char *argv[])
{
    logFile = create_log_file();
    load_users();

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
    puts("Waiting for incoming connections...");

    char *log_connect = (char*)malloc(130 * sizeof(char));
    char *log_message = (char*)malloc((BUFFER_SIZE + 130) * sizeof(char));

    socklen_t client_len = sizeof(struct sockaddr_in);
    while ( (client_fd = accept(server_fd, (struct sockaddr *) &client, &client_len)) ) {

        snprintf(
                log_connect,
                300,
                "User with ip address %s connected to the server",
                inet_ntoa(client.sin_addr)
        );

        log_to_file(log_connect);

        char user_prompt_username[BUFFER_SIZE] = "\nUsername: ";

        if ((send(client_fd, user_prompt_username, BUFFER_SIZE, 0)) < 0) {
            on_error("Username prompt failed\n");
        }

        memset(user_prompt_username, 0, BUFFER_SIZE) ;

        if ((recv(client_fd, user_prompt_username, BUFFER_SIZE, 0)) < 0) {
            on_error("Couldn't read username\n");
        }
        user_prompt_username[strlen(user_prompt_username) - 1] = 0;

        printf("\n%s", user_prompt_username);

        char user_prompt_password[BUFFER_SIZE] = "\nPassword: ";

        if ((send(client_fd, user_prompt_password, BUFFER_SIZE, 0)) < 0) {
            on_error("Password prompt failed\n");
        }

        memset(user_prompt_password, 0, BUFFER_SIZE);

        if ((recv(client_fd, user_prompt_password, BUFFER_SIZE, 0)) < 0) {
            on_error("Couldn't read password\n");
        }
        user_prompt_password[strlen(user_prompt_password) -1] = 0;

        printf("\n%s", user_prompt_password);

        if (check_credentials(user_prompt_username, user_prompt_password) < 0) {
            on_error("Couldn't find user.\n");
        };

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

void handle_request(thread_args args){
    char *log_message = (char*)malloc((BUFFER_SIZE + 130) * sizeof(char));
    int err;
    char buf[BUFFER_SIZE];
    char user_prompt_username[BUFFER_SIZE] = "\nUsername: ";
    int client_fd = args.socket_addr;


    err = send(client_fd, user_prompt_username, BUFFER_SIZE, 0);
    if (err < 0) on_error("Username prompt failed\n");

    memset(user_prompt_username, 0, BUFFER_SIZE) ;

    int username_length = recv(client_fd, user_prompt_username, BUFFER_SIZE, 0);
    if (username_length < 0) on_error("Couldn't read username\n");
    user_prompt_username[strlen(user_prompt_username) - 1] = 0;

    printf("\n%s", user_prompt_username);

    char user_prompt_password[BUFFER_SIZE] = "\nPassword: ";

    err = send(client_fd, user_prompt_password, BUFFER_SIZE, 0);
    if (err < 0) on_error("Password prompt failed\n");

    memset(user_prompt_password, 0, BUFFER_SIZE);
    int password_length = recv(client_fd, user_prompt_password, BUFFER_SIZE, 0);
    if (password_length < 0) on_error("Couldn't read password\n");
    user_prompt_password[strlen(user_prompt_password) -1] = 0;

    printf("\n%s", user_prompt_password);

    err = check_credentials(user_prompt_username, user_prompt_password);
    if (err < 0) on_error("Couldn't find user.\n");

    while (1) {
        int read = recv(client_fd, buf, BUFFER_SIZE, 0);
        snprintf(
                log_message,
                BUFFER_SIZE + 100,
                "%s send message: %s",
                inet_ntoa(args.client.sin_addr),
                buf
        );

        log_to_file(log_message);

        if (!read) break;
        if (read < 0) on_error("Client read failed\n");

        err = send(client_fd, buf, read, 0);
        if (err < 0) on_error("Client write failed\n");
    }

}
