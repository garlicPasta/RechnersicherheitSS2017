/* ex: set tabstop=2 expandtab: */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>

#include <pwd.h>

#define 		PORT_NO 							12345

/* Password DB */
#define     PWD_FILE              "/etc/ushoutd.passwd"
#define 		PWD_FILE_MAX_SIZE 		1024
#define 		PWD_MAX_PW_SIZE 			100
int 				passwordDBLoad(void);
int 				passwordDBCheckpass(const char* user, const char* pass);



/* Server (network) */
#define 		N_NET_RECEIVE_BUFFER 	1024
#define 		N_NET_WRITE_BUFFER    1024
#define 		N_CMD_BUFFER 					1024
#define 		PROMPT_PASSWORD 			"Username:Password: "

#define 		REPLY_WRONG_PW    		"Authentication Failed.\n"
#define 		REPLY_AUTH_OK		  		"Authenticated.\n"
void 				serverRunLoop(const int portNo);
int 				serverDoAnswer(const int clientSocket, const char* message, const ssize_t length);

/* Client */
typedef enum {NONE, NEW, AUTHENTICATED} CLIENT_STATE;
typedef struct
{
  CLIENT_STATE state;
  char* username;
} Client;

int         clientNewClient(const int clientSocket, Client* to);
void        clientDestroy(Client* me);
int         clientOnAuthMessage(Client* me, const int clientSocket, const char* message, const size_t _length);

/* Logging */
#define     LOG_FILE            "/ushoutd.log"
FILE*       __logFile = NULL;

int         logOpen(const char* logfile, int mygid);
void        logDoLog(const char* fmt, ...);

/* misc */
#define     MISC_CHROOT         "/var/ushoutd/"
#define     MISC_USER           "_ushoutd"
int         miscDropPriviliges(int, int);
int         miscDoChroot(const char*);
int         miscGetMyUidGid(const char*, int*,  int*);



int main(void)
{
  int myuid = getuid();
  int mygid = getgid();

  if(0 != miscGetMyUidGid(MISC_USER, &myuid, &mygid))
  {
    exit(1);
  }

  fprintf(stderr, "[Startup] Loading password database\n");
  if(0 != passwordDBLoad())
  {
    exit(1);
  }

  fprintf(stderr, "[Startup] Chroot to %s\n", MISC_CHROOT);
  if(0 != miscDoChroot(MISC_CHROOT))
  {
    exit(1);
  }


  fprintf(stderr, "[Startup] Opening log file %s\n", LOG_FILE);
  if(0 != logOpen(LOG_FILE, mygid))
  {
    exit(1);
  }
  logDoLog("[Startup] Opened log file\n");

   // Zum finden der ausgefuehrten systemcalls haben wir das Program mit
   // ktrace -tc ./ushoultd gestart.
   // Im Anschluss wurden haben wir uns mit
   // kdump | awk '{print $4}' | sed -e s/(.*//g | sort | uniq
   // die einzellen syscalls ausgeben lassen. Diese haben wir dann mit den
   // ensprechenden promises aus 'man pledge' abgeglichen
   
  if(pledge("getpw stdio inet exec rpath wpath unix prot_exec flock id", NULL) == -1)
  {
	  exit(1);
  }

  fprintf(stderr, "[Startup] Dropping privileges to user %s\n", MISC_USER);
  logDoLog("[Startup] Dropping privileges to user %s\n", MISC_USER);
  if(0 != miscDropPriviliges(myuid, mygid))
  {
    exit(1);
  }



	int listenSocket;
	struct sockaddr_in server;

  fprintf(stderr, "[Startup] Open listening socket on port %d\n", PORT_NO);
  logDoLog("[Startup] Open listening socket on port %d\n", PORT_NO);
	listenSocket = socket(AF_INET, SOCK_STREAM, 0);
	if(0 > listenSocket)
	{
		perror("opening socket");
		exit(1);
	}
  
  const int enable = 1;
  if (0 != setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)))
  {
    perror("Setting SO_REUSEADDR to socket");
    exit(1);
	}

	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(PORT_NO);

	if( 0 != bind(listenSocket, (struct sockaddr*) &server, sizeof(server)) )
	{
		perror("binding socket");
		exit(1);
	}
	
	if( 0 != listen(listenSocket, 1))
	{
		perror("listening on socket");
		exit(1);
	}

  fprintf(stderr, "[Startup] Done\n");
  fprintf(stderr, "[Startup] Starting Server\n");

	serverRunLoop(listenSocket);

	exit(0);
}

/* Server Begin */

void serverRunLoop(int listenSocket)
{
  fd_set activeConnections;
  Client clients[FD_SETSIZE];
	explicit_bzero(clients, sizeof(clients));

  FD_ZERO(&activeConnections);
  FD_SET(listenSocket, &activeConnections);

	for(;;)
	{
    fd_set readyConnections = activeConnections;

    if(0 > select(FD_SETSIZE, &readyConnections, NULL, NULL, NULL)) 
    {
      perror("select");
      exit(1);
    }

    int i = 0;
    for(i=0; i < FD_SETSIZE; i++)
    {
      if(FD_ISSET(i, &readyConnections))
      {
        if(i == listenSocket)
        {
          Client new;
		      int clientSocket = accept(listenSocket, 0, 0);
	      	if(-1 == clientSocket)
		      {
			      perror("accepting connection");
			      continue;
		      }
		      if(0 > clientNewClient(clientSocket, &new))
		      {
		    	  perror("handling new connection");
            close(clientSocket);
		    	  continue;
		      }

          clients[clientSocket] = new;
          FD_SET(clientSocket, &activeConnections);
        }
        else
        {
	    	  char buffer[N_NET_RECEIVE_BUFFER+1]; // always keep one \0
		      explicit_bzero(buffer, sizeof(buffer)-1);
		      ssize_t nRead = 0;
		      int status = 0;

		     	nRead = read(i, buffer, sizeof(buffer)-1);

		     	if(0 > nRead)
		     	{
		     		perror("reading from client");
		     	}		
		     	else if(0 == nRead)
		     	{
		     		fprintf(stderr, "close connection\n");	
            close(i);
            logDoLog("[User] User %s left.\n", clients[i].username); 
            clientDestroy(&clients[i]);
            FD_CLR(i, &activeConnections);
		     	}
		     	else
		     	{
            if(AUTHENTICATED != clients[i].state)
		     		{
              status = clientOnAuthMessage(&clients[i], i, buffer, nRead);
		     		  if(-1 == status)
		     		  {
                close(i);
                FD_CLR(i, &activeConnections);
                logDoLog("[User] Killed unauthenticated client for invalid message\n");
                clientDestroy(&clients[i]);

		     			  status |= -1;	
		     		  }
            }
            else
            {
              logDoLog("[Msg] User %s wrote:\n%s\n", clients[i].username, buffer);
              int j = 0;
              for(j=0; j < FD_SETSIZE; j++)
              {
                if(FD_ISSET(j, &activeConnections))
                {
                  if(j == listenSocket)
                  {
                    continue;
                  }
                  if( -1 == serverDoAnswer(j, buffer, nRead))
                  {
                    close(j);
                    logDoLog("[User] User %s left.\n", clients[j].username); 
                    clientDestroy(&clients[j]);
                    FD_CLR(j, &activeConnections);
                  }
                }
              }
		     	  }
		        explicit_bzero(buffer, sizeof(buffer));
          }
        }
      }
    }
	}
}


int serverDoAnswer(const int clientSocket, const char* message, const ssize_t length)
{
	ssize_t nWritten = 0;
	ssize_t nToBeWritten = length;
	int status = 0;
	do
	{
		const int written = write(clientSocket, message+nWritten, nToBeWritten);
		if(-1 == written)
		{
			perror("writing to socket");
			status = -1;
		}
		nWritten += written;
		nToBeWritten -= written;
	}while(0 < nToBeWritten && -1 != status);

	return status;
}

/* Server end */

/* Connection begin */

int clientNewClient(const int clientSocket, Client* to)
{
  if(NULL == to)
  {
    exit(2);
  }
  to->state = NEW;
  to->username = NULL;

	return serverDoAnswer(clientSocket, PROMPT_PASSWORD, sizeof(PROMPT_PASSWORD));
}

void clientDestroy(Client* me)
{
  if(NULL == me)
  {
    exit(2);
  }
  me->state = NONE;
  free(me->username);
  me->username = NULL;
}

int clientOnAuthMessage(Client* me, const int clientSocket, const char* message, const size_t _length)
{
	char command[N_CMD_BUFFER];
  size_t length = _length;

	// perform some basic input validation
	// (using nc as client, <message> will at least contain a newline '\n' character)
	if(1 >= length)
	{
		fprintf(stderr, "Received empty message, terminating connection...\n");
		return -1;
	}
	else if(N_CMD_BUFFER < length)
	{
		fprintf(stderr, "Received message too large, terminating connection...\n");
		return -1;
	}

	if('\n' != message[length-1])
	{
		fprintf(stderr, "Received invalid message: should end with newline, terminating connection\n");
		return -1;
	}

	memcpy(command, message, length);
  // strip newline at end of command
  command[length-1] = '\0';
	length = length-1;

	int result = 0;
	switch(me->state)
	{
		case NEW:
    {
        char* pass = command;
        char* user = strsep(&pass, ":");
        if( 0 == passwordDBCheckpass(user, pass))
				{
					me->state = AUTHENTICATED;
          me->username = strdup(user);
					result |= serverDoAnswer(clientSocket, REPLY_AUTH_OK, sizeof(REPLY_AUTH_OK));
          logDoLog("[User] Authentication succeeded for user %s\n", user);
          fprintf(stderr, "Authentication succeeded for user %s\n", user);
				}
				else
				{
					logDoLog("[User] Authentication failed for user %s\n", user);
					fprintf(stderr, "Authentication failed for user %s\n", user);
					result |= serverDoAnswer(clientSocket, REPLY_WRONG_PW, sizeof(REPLY_WRONG_PW));
					result |= serverDoAnswer(clientSocket, PROMPT_PASSWORD, sizeof(PROMPT_PASSWORD));
				}
				explicit_bzero(command, length);
    }
		break;
    
		default:
			fprintf(stderr, "Did end up in invalid state %d: this should never happen!\n", me->state);
			exit(-1);
		break;
	}

	return result;
}


/* Password DB begin */
#include <db.h>

DB* __pwdb = NULL;

int passwordDBLoad(void)
{
  __pwdb = dbopen(PWD_FILE, O_RDONLY, 0, DB_HASH, NULL);
  if(NULL == __pwdb)
  {
    perror("Opening pw database");
    return 1;
  }
  return 0;
}

int passwordDBCheckpass(const char* user, const char* pass)
{
  DBT key;
  key.data = (void*)user;
  key.size = strlen(user)+1;

  DBT data;
  data.data = NULL;
  data.size = 0;

  int status = __pwdb->get(__pwdb, &key, &data, 0);
  if(-1 == status)
  {
    perror("Checking password, terminating");
    exit(-1);
  }

  if(0 == status)
  {
    if(0 == crypt_checkpass(pass, data.data))
    {
      return 0;
    }
  }
  else
    crypt_checkpass("","");

  return 1;
}

/* Password DB end */


/* Log begin */

int logOpen(const char* logfile, int mygid){
  
  // NOTE: there is TOCTTOU vulnerability here if a malicious
  // program already has an open file descriptor on this file
  
  const int flags = S_IRUSR | S_IWUSR | S_IWGRP;

  umask(flags);

  __logFile = fopen(logfile, "aw+");
  if(NULL == __logFile)
  {
    perror("Opening log file\n");
    return 1;
  }

  int fd = fileno(__logFile);

  if(0 != fchown(fd, 0, mygid))
  {
    perror("Setting owner of file");
    return 1;
  }

  if(0 != fchmod(fd, flags))
  {
    perror("Setting mode of file");
    return 1;
  }

  return 0;
}

void logDoLog(const char* format, ...)
{
    va_list args;
    va_start(args, format);

    vfprintf(__logFile, format, args);
    fflush(__logFile);

    va_end(args);
}

/* Log end */


/* Misc begin */

int miscGetMyUidGid(const char* username, int* uid, int* gid)
{
  int bufsize = sysconf(_SC_GETPW_R_SIZE_MAX);
  if(-1 == bufsize)
  {
    perror("Getting _SC_GETPW_R_SIZE_MAX");
    exit(1);
  }

  char* buffer = calloc(sizeof(char), bufsize);
  if(NULL == buffer)
  {
    perror("cAllocating _SC_GETPW_R_SIZE_MAX bytes");
    exit(1);
  }
        
  struct passwd pwd;
  struct passwd* result = NULL;
  
  if(0 == getpwnam_r(username, &pwd, buffer, bufsize, &result))
  {
    if(NULL != result)
    {
      *uid = pwd.pw_uid;
      *gid = pwd.pw_gid;
    }
    else
    {
      fprintf(stderr, "Could not find user named %s\n", MISC_USER);
      exit(1);
    }
  }
  free(buffer); 
  buffer = NULL;
  result = NULL;

  return 0;
}

int miscDropPriviliges(int uid, int gid)
{
  //fprintf(stderr, "[Startup] Dropping privileges to uid %d, gid %d\n", uid, gid);
  if(0 != initgroups(MISC_USER, gid))
  {
    perror("Setting group access list");
    return 1;
  }
  if(0 != setgid(gid))
  {
    perror("Setting group id");
    return 1;
  }
  if(0 != setuid(uid))
  {
    perror("Setting user id");
    return 1;
  }

  /* sanity check */
  if(0 == setuid(0) || 0 == setgid(0))
  {
    fprintf(stderr, "Failed to drop privileges");
    return 1;
  }

  return 0;
}

int miscDoChroot(const char* chrootdir)
{
  //fprintf(stderr, "[Startup] chroot to %s\n", MISC_CHROOT);
  if(0 != chdir(chrootdir))
  {
    perror("chdir");
    return 1;
  }
  if(0 != chroot(chrootdir))
  {
    perror("chroot");
    return 1;
  }

  return 0;
}

/* Misc end */


