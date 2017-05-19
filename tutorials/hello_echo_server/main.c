#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>

int main() {

  char str[100];

  // File Descriptors to be used
  int listen_fd, comm_fd;

  // Struct to hold IP Address and Port Numbers
  struct sockaddr_in servaddr;

  /*
    Each server needs to “listen” for connections.
    The function creates a socket with AF_INET ( IP Addressing )
    and of type SOCK_STREAM. Data from all devices wishing
    to connect on this socket will be redirected to listen_fd.
  */
  listen_fd = socket(AF_INET, SOCK_STREAM, 0);

  // Clear servaddr ( Mandatory ).
  bzero(&servaddr, sizeof(servaddr));

  /*
    Set Addressing scheme to – AF_INET ( IP )
    Allow any IP to connect – htons(INADDR_ANY)
    Listen on port 22000 – htons(22000)
  */
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons(22000);

  /*
    Prepare to listen for connections from address/port specified
    in sockaddr ( Any IP on port 22000 ).
  */
  bind(listen_fd, (struct sockaddr *) &servaddr, sizeof(servaddr));

  /*
    Start Listening for connections , keep at the most 10 connection
    requests waiting.If there are more than 10 computers wanting to
    connect at a time, the 11th one fails to.
  */
  listen(listen_fd, 10);

  /*
    Accept a connection from any device who is willing to connect,
    If there is no one who wants to connect , wait. A file descriptor
    is returned. This can finally be used to communicate , whatever is
    sent by the device accepted can be read from comm_fd, whatever is
    written to comm_fd is sent to the other device.
  */
  comm_fd = accept(listen_fd, (struct sockaddr*) NULL, NULL);

  while(1) {
    bzero( str, 100);
    read(comm_fd,str,100);
    printf("Echoing back - %s",str);
    write(comm_fd, str, strlen(str)+1);
  }

  return 1;
}
