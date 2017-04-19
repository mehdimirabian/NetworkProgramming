/*
*   File:       myclient.c
*   Assignment: Homework1
*   Name:       Mohammad Mirabian
*   CruzID:     mmirabia
*/

#include "myunp.h"
#include <time.h>
#include <string.h>
#include <errno.h>
#include <linux/limits.h>

int main(int argc, char **argv)
{

	int					sockfd, n;
	char				recvline[MAXLINE + 1];
	struct sockaddr_in	cliaddr;

	if (argc != 3){
		err_quit("usage: a.out <IPaddress> <Portnumber>");
	}
	
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	
	if (sockfd < 0){
		err_sys("socket error");
	}
	
	
	cliaddr.sin_family = AF_INET;
	uint16_t portNumber =(uint16_t) atoi(argv[2]);
	if (portNumber <= 0){
		err_quit("atoi arv2 %s", argv[2]);     /* daytime server */
	}
	cliaddr.sin_port = htons(portNumber);
	if (inet_pton(AF_INET, argv[1], &cliaddr.sin_addr) <= 0)
		err_quit("inet_pton error for %s", argv[1]);

	
	//Start by asking for an input(command) from the client.
	printf("Type in a command: ");
	char inCommand[MAXLINE];
	scanf("%s", inCommand);	


	if (connect(sockfd, (SA *) &cliaddr, sizeof(cliaddr)) < 0) {
	printf("errno = %d (%s)\n", errno, strerror(errno));	
	err_sys("connect error");
	}
	Write(sockfd, inCommand, MAXLINE);
	
	//Here we finally recive the output of command from the server
	//and print it
	while((n = Read(sockfd, recvline, strlen(recvline) + 1)) != 0) {

		if (fputs(recvline, stdout) == EOF)
			err_sys("fputs error");

	}

	exit(0);

}
