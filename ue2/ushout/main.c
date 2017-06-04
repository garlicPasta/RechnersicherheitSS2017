#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <time.h>
#include <arpa/inet.h>
#include <memory.h>
#include <pthread.h>
#include <unistd.h>    //write

#define BUFFER_SIZE 1024
#define BUFFER_LOG_CONNECTION_SIZE 130
#define CONCURRENT_CLIENT_COUNT_MAX 20
#define MAX_USERS 100

// Production:
//#define LOG_PATH "/var/log/ushoutd.log"
//#define PASS_PATH "/etc/ushoutd.passwd"

// Debug Jakob:
//#define LOG_PATH "/home/jakob/uni/rechnersicherheit/ue2/ushout/ushoutd.log"
//#define PASS_PATH "/home/jakob/uni/rechnersicherheit/ue2/ushout/ushoutd.passwd"

// Debug Ramdane:
#define LOG_PATH "/Users/rs/Documents/Uni/SS17/Rechnersicherheit/ue/git-repo/ue2/ushout/ushoutd.log"
#define PASS_PATH "/Users/rs/Documents/Uni/SS17/Rechnersicherheit/ue/git-repo/ue2/ushout/ushoutd.passwd"

#define on_error(...) { printf("Error!"); fprintf(stderr, __VA_ARGS__); fflush(stderr); fclose(logFile); exit(1); }

static FILE *logFile;
FILE * create_log_file(void);
static int total_users_count;

typedef struct {
    char *username;
    char *password;
} user;

void log_to_file(char *message);
void get_date(char *buffer);
void *handle_request(void *);

user *createUser(char *username, char *password) {

    user *user = malloc(sizeof(user));

    user->username = strdup(username);
    user->password = strdup(password);
    return user;
}

static user *user_list[MAX_USERS];
static int clients[CONCURRENT_CLIENT_COUNT_MAX];

void load_users(){
    FILE * fp;
    char * line = NULL;
    size_t len = 0;
    ssize_t read;

    fp = fopen(PASS_PATH, "r");
    if (fp == NULL)
        exit(EXIT_FAILURE);

    total_users_count = 0;
    int user_count=0;
    while ((read = getline(&line, &len, fp)) != -1) {
        char *token;
        char *user_data[2];
        int i = 0;
        //TODO: fix buffer overflow, if passwords has multiple ":"
        while ((token = strsep(&line, ":"))) {
            user_data[i++] = token;
        }
        user_data[1][strcspn(user_data[1], "\n")] = 0;
        user *u = createUser(user_data[0], user_data[1]);
        user_list[user_count++] = u;
    }

    total_users_count = user_count;

    fclose(fp);
    if (line)
        free(line);

}

//TODO: improve inefficient user check
int check_credentials(char *username, char *password) {

  for (int i = 0; i < total_users_count; i++) {
    user *u = user_list[i];

    if (strcmp(username, u->username) == 0 && strcmp(password, u->password) == 0) {
      return 1;
    }
  }

  return 0;
}

int main (int argc, char *argv[])
{
    logFile = create_log_file();
    load_users();

    if (argc < 2) on_error("Usage: %s [port]\n", argv[0]);
    int port = atoi(argv[1]);

    int server_fd, client_fd, *new_sock;
    struct sockaddr_in server, client;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) on_error("Could not create socket\n");

    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = htonl(INADDR_ANY);

    int opt_val = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof opt_val);

    if ((bind(server_fd, (struct sockaddr *) &server, sizeof(server))) < 0) {
        on_error("Could not bind socket. Check if port is available.\n");
    }

    // Listen
    if ((listen(server_fd, CONCURRENT_CLIENT_COUNT_MAX)) < 0) {
        on_error("Could not listen on socket\n");
    }

    printf("Server is listening on %d\n", port);
    puts("Waiting for incoming connections...");

    char *log_connect = (char*)calloc(BUFFER_LOG_CONNECTION_SIZE, sizeof(char));
    char *log_message = (char*)calloc((BUFFER_SIZE + BUFFER_LOG_CONNECTION_SIZE), sizeof(char));

    socklen_t client_len = sizeof(struct sockaddr_in);
    while ( (client_fd = accept(server_fd, (struct sockaddr *) &client, &client_len)) ) {
        puts("Connection accepted");

        pthread_t sniffer_thread;
        new_sock = malloc(1);
        *new_sock = client_fd;

        if (pthread_create( &sniffer_thread , NULL ,  handle_request , (void*) new_sock) < 0) {
            on_error("could not create thread");
        }

        snprintf(
                log_connect,
                BUFFER_LOG_CONNECTION_SIZE,
                "Client with ip address %s connected to the server",
                inet_ntoa(client.sin_addr)
        );

        log_to_file(log_connect);
    }

    if (client_fd < 0) {
        on_error("Connecting client failed.");
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


int user_prompt(int client_fd) {
    char *message, username[BUFFER_SIZE], password[BUFFER_SIZE];

    message = "Type username:\n";
    write(client_fd , message , strlen(message));

    memset(username, 0, BUFFER_SIZE);
    if ((recv(client_fd, username, BUFFER_SIZE, 0)) < 0) {
        return 0;
    }
    username[strlen(username) - 1] = 0;

    message = "Type password:\n";
    write(client_fd , message , strlen(message));

    memset(password, 0, BUFFER_SIZE);

    if ((recv(client_fd, password, BUFFER_SIZE, 0)) < 0) {
        return 0;
    }
    password[strlen(password) -1] = 0;

    if (!check_credentials(username, password)) {
        printf("Credentials wrong.\n");
        // todo: remove thread or reprompt for login details
        return 0;
    } else {
        printf("%s logged in successfully\n", username);
        return 1;
    }
}


void *handle_request(void *server_fd) {

    //Get the socket descriptor
    int client_fd = *(int*)server_fd;
    int read_size;
    char client_message[BUFFER_SIZE];

    while (!user_prompt(client_fd)) {
        // user needs to log in successfully to continue
    }

    // User logged in successfully

    clients[0] = client_fd;

    //Receive a message from client
    while ((read_size = recv(client_fd , client_message , 2000 , 0)) > 0 ) {

      // snprintf(
      //         log_message,
      //         BUFFER_SIZE + 100,
      //         "%s send message: %s",
      //         inet_ntoa(client.sin_addr),
      //         client_message
      // );

      // log_to_file(log_message);

        int clients_len = (int)(sizeof(clients) / sizeof(clients[0]));

        for (int i = 0; i < clients_len; i++) {

          //Send the message back to client
          write(clients[i] , client_message , strlen(client_message));
 
          memset(client_message, 0, strlen(client_message));
        }

    }

    if (read_size == 0) {
        puts("Client disconnected");
        fflush(stdout);
    } else if (read_size == -1) {
        perror("recv failed");
    }

    //Free the socket pointer
    free(server_fd);

    return 0;
}
