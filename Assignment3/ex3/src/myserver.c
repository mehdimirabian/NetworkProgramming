
/* File Name: myserver.c
Name :     Mohammad Mirabian
Assignment:Homework 3
CruzID:    mmirabia
*/


#include "myunp.h"
#include <sys/stat.h>


#define MAXIMUM_DATA 1024

typedef struct
{
	unsigned int type;///=4 respnse file data
	unsigned int seqnumber;
	long long offset;
	long long chunksize;
}UDP_HEADER;


typedef struct
{
	UDP_HEADER header;
	char data[MAXIMUM_DATA];

}UDP_PACKET;





int connection(UDP_PACKET *myConnection,int connectionSize,UDP_PACKET *response)
{
	struct stat stats={};

	if(connectionSize>=sizeof(*myConnection))
	return -1;

	if(connectionSize<=0)
	return -1;

	if(connectionSize<sizeof(myConnection->header))
	return -1;

	if((myConnection->header.type!=1)&&(myConnection->header.type!=3))
	return -1;

	response->header.type=0;
	response->header.seqnumber=myConnection->header.seqnumber;
	response->header.offset=0;
	response->header.chunksize=0;

	if(myConnection->header.type==1)
	{
		myConnection->data[connectionSize-sizeof(myConnection->header)]=0;

		if(-1==stat(myConnection->data,&stats))
		{
			strcpy(response->data,"stat error");
			return sizeof(response->header)+strlen(response->data);
		}
		else
		{
			response->header.type=2;
			response->header.chunksize=stats.st_size;

			return sizeof(response->header);
		}
	}



	if(myConnection->header.type==3)
	{
		FILE * fp=0;
		int readsize=0;

		myConnection->data[connectionSize-sizeof(myConnection->header)]=0;


		if(myConnection->header.chunksize>MAXIMUM_DATA)
		{
			strcpy(response->data,"chunk is too big");
			return sizeof(response->header)+strlen(response->data);
		}


		fp=fopen(myConnection->data,"rb");

		if(0==fp)
		{
			strcpy(response->data,"fopen error");
			return sizeof(response->header)+strlen(response->data);
		}


		if(0!=fseek(fp,myConnection->header.offset,SEEK_SET))
		{
			fclose(fp);

			strcpy(response->data,"fseek error");

			return sizeof(response->header)+strlen(response->data);
		}

		readsize=fread(response->data,1,myConnection->header.chunksize,fp);



		fclose(fp);

		response->header.type=4;
		response->header.offset=myConnection->header.offset;
		response->header.chunksize=readsize;


		return sizeof(response->header)+readsize;
	}

	return -1;
}

int main(int argc, char **argv)
{
	int reuse = 1;
	struct sockaddr_in servaddr={};
	int port;
	int sockfd;

	if(argc<2)
	{
		err_quit("format is ./myserver <port-number>\n");
	}

	port=strtol(argv[1],0,10);

	sockfd=Socket( AF_INET, SOCK_DGRAM, 0);


	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int)) < 0)
	{
		err_quit("setsockopt error\n");
	}


	servaddr.sin_addr.s_addr=INADDR_ANY;//any ip addresses
	servaddr.sin_port=htons(port);

	Bind(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr) );


	while(1)
	{
 		struct sockaddr cliaddr;
		socklen_t cliaddrsz=sizeof(cliaddr);
		int responsesize=0;
		int myConnectionsize=0;
		UDP_PACKET myConnection;
		UDP_PACKET response;

		myConnectionsize=recvfrom(sockfd,&myConnection,sizeof(myConnection),0,&cliaddr,&cliaddrsz);

		responsesize=connection(&myConnection,myConnectionsize,&response);

		if(responsesize>0)
		{///send response
			sendto(sockfd,&response,responsesize,0,&cliaddr,cliaddrsz);
		}

	}

	Close(sockfd);

    return 0;
}

