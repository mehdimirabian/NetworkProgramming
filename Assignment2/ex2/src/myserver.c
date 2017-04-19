/*
*   File:       myserver.c
*
*   Purpose:    This is a skeleton file for a server.
*/

#include "myunp.h"
#include "myprotocol.h"
#include <sys/stat.h>

int processClient(int sockfd)
{
///on any error just return -1, no need to kill server...
	PACKET_HEADER header;
	char filename[FILENAME_MAX];
	struct stat filestats={};

	if(-1==recvAll(sockfd,(char*)&header,sizeof(header)))
	return -1;

	if(header.textlength>FILENAME_MAX-1)
	{
		printf("Error, too long file name received\n");///non fatal error
		return-1;
	}

	if(-1==recvAll(sockfd,(char*)filename,header.textlength))
	return -1;

	filename[header.textlength]=0;

	if(-1==stat(filename,&filestats))
	{
		sendErrorString(sockfd,"stat error");
		return-1;
	}

	if(header.packettype==0)///file size request
	{
		PACKET_HEADER response={};
		response.packettype=0;///file size request success
		response.transfersize=filestats.st_size;

		if(-1==sendAll(sockfd,(char*)&response,sizeof(response)))return-1;
	}
	else
	if(header.packettype==1)///file data request
	{
		FILE * fp=fopen(filename,"rb");

		if(0==fp)
		{
			sendErrorString(sockfd,"fopen error");
			return-1;
		}

		if(header.transfersize==-1)///all to the end
		{
			header.transfersize=filestats.st_size;
		}

		if(0!=fseek(fp,header.startoffset,SEEK_SET))
		{
			fclose(fp);
			sendErrorString(sockfd,"fseek error");
			return-1;
		}

		{
			PACKET_HEADER response={};
			response.packettype=1;///file data request success
			response.startoffset=header.startoffset;
			response.transfersize=header.transfersize;

			if(-1==sendAll(sockfd,(char*)&response,sizeof(response)))///stop reading if we cannot send...
			{
				printf("cannot send\n");
				fclose(fp);
				return -1;
			}
		}

		{
			char chunk[4096];

			long long size=header.transfersize;

			printf("start transfer of %lld\n",size);

			while(size>0)
			{
				long long chunksize=size;
				int res;
				if(chunksize>sizeof(chunk))chunksize=sizeof(chunk);

				res=fread(chunk,1,chunksize,fp);


				if(-1==sendAll(sockfd,chunk,res))///stop processing if we cannot send...
				{
					printf("cannot send more\n");
					fclose(fp);
					return -1;
				}

				if(res!=chunksize)///stop processing if we cannot read as much as we want...
				{
					printf("cannot read more\n");
					fclose(fp);
					return -1;
				}

				size-=res;
			}
		}

		fclose(fp);
	}
	else
	{
		sendErrorString(sockfd,"bad packet type error");
		return-1;
	}

	return 0;
}

int main(int argc, char **argv)
{
	int port;
	int sock;

	if(argc<2)
	{
		err_quit("USAGE myserver <port-number>\n");
	}

	port=strtol(argv[1],0,10);

	sock=Socket( AF_INET, SOCK_STREAM, 0);

	{
		int enable = 1;///enable address reuse , so program can bind to the same port after ...crash
		if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
		{
			err_quit("bad setsockopt\n");
		}
	}

	{
		struct sockaddr_in saddr={};

		saddr.sin_addr.s_addr=INADDR_ANY;
		saddr.sin_port=htons(port);

		Bind(sock, (struct sockaddr*)&saddr, sizeof(saddr) );
	}

	Listen(sock,20);///put big backlog so lots of clients will stay waiting, but connected

	while(1)
	{
 		struct sockaddr clientaddr;
		socklen_t clientaddrsz=sizeof(clientaddr);

		int clientSock=Accept(sock, &clientaddr, &clientaddrsz);

		processClient(clientSock);

		Close(clientSock);
	}

	Close(sock);

    return 0;
}

