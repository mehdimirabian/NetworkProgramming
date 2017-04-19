/*
*   File:       myserver.c
*   Assignment  Homework1
*   Name:       Mohammad Mirabian
*   CruzID:     mmirabia
*/

#include "myunp.h"
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#include <linux/limits.h>
int main(int argc, char **argv)
{
	int    listenfd, connfd, pCloseStat;
 	struct sockaddr_in   servaddr;
	char   inCommand[MAXLINE];
	char   executedCom[PATH_MAX];
	FILE *fp;

	// Create the socket and store it in
	// listenfd
	listenfd = Socket(AF_INET, SOCK_STREAM,0);
 	//the server uses ipv4
	servaddr.sin_family = AF_INET;
	//server accepts from any ip addresses
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY); 

	uint16_t portNumber =(uint16_t) atoi(argv[1]);
	servaddr.sin_port = htons(portNumber); 

	int mybind = Bind(listenfd,(SA *)  &servaddr, sizeof(servaddr));


	Listen(listenfd, 10);
	 
	for(;;){
	
		connfd = Accept(listenfd, (SA *) NULL, NULL); 
		Read(connfd,inCommand, MAXLINE);
		//now the sent string (command) from the client is here
		//stored in inCommand. Now it's time to execute the
		//command itself using popen:
		fp = popen(inCommand, "r");
		if (fp == NULL)
			printf("File pointer Error");
		while(fgets(executedCom, sizeof(inCommand)-1, fp) != NULL){
			printf("%s", executedCom);
			Write(connfd, executedCom, strlen(executedCom) + 1); 
		}
		

		Close(connfd);
		pCloseStat = pclose(fp);
		if (pCloseStat == -1)
			printf("pclose error");	
	
	
	}
 
	return 0;
}
