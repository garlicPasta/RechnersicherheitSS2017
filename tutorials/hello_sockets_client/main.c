#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include<string.h>

int main(int argc, char **argv) {

    int sockfd, n;
    char sendline[100];
    char recvline[100];
    struct sockaddr_in servaddr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    bzero(&servaddr, sizeof servaddr);

    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(22000);

    /*
      Set IP address in servaddr to “127.0.0.1” ( computers way of s
      aying myself ) since our server is also on the same machine .
      The address in servaddr needs to be in integer format , hence
      the function inet_pton
    */
    inet_pton(AF_INET, "127.0.0.1", &(servaddr.sin_addr));

    /*
      Connect to the device whose address and port number is specified in
      servaddr.
    */
    connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));

    /*
      The Client then does following forever.
        Clear sendline and recvline
        read string from stdin in sendline ( stdin is 0 for standard input )
        write sendline in sockfd
        read from sockfd in srecvline
        Display recvline
    */
    while(1) {
        bzero(sendline, 100);
        bzero(recvline, 100);
        fgets(sendline,100,stdin); /*stdin = 0 , for standard input */

        write(sockfd, sendline, strlen(sendline)+1);
        read(sockfd, recvline, 100);
        printf("%s", recvline);
    }

}
