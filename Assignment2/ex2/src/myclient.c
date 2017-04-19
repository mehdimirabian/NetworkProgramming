/*
*   File:       myclient.c
*
*   Purpose:    This is a skeleton file for a client.
*/

#include "myunp.h"
#include "myprotocol.h"
#include "errno.h"
#include <time.h>
#include <pthread.h>


#define MAX_SERVERS 20

struct sockaddr_in serveraddress[MAX_SERVERS];
int servercount=0;



typedef struct
{
int serverindex;
long long startoffset;
long long transfersize;
FILE * fp;
pthread_mutex_t *pmutex;
char *filename;
int ready;
}threadrequesttype;


long long getfilechunk(threadrequesttype* req);
long long getfilesize(const char * filename);



void * chunkdownloader(void *arg)
{
	getfilechunk((threadrequesttype*)arg);

	return 0;
}

int main(int argc, char **argv)
{
	int numchunks=0;

	if(argc<3)
	{
		err_quit("USAGE myclient <server-info.text> <num-connections>\n");
	}


	{
		char fileline[1024];
		FILE * fp=fopen(argv[1],"rb");
		if(!fp)
		{
			err_quit("Cannot open %s", argv[1]);
		}


		while(fgets(fileline,sizeof(fileline),fp)!=0)
		{
			char ip[1024];
			int port=0;

			if(sscanf(fileline," %s %d",ip,&port)!=2)
				err_quit("Bad line in file\n");


			if(servercount<MAX_SERVERS)
			{
				if(!strcmp("localhost",ip))
				{
					serveraddress[servercount].sin_addr.s_addr=htonl(INADDR_LOOPBACK);
					serveraddress[servercount].sin_family=AF_INET;
					serveraddress[servercount].sin_port=htons(port);
					servercount++;
				}
				else
				if(0!= inet_aton(ip, &serveraddress[servercount].sin_addr))///good address
				{
					serveraddress[servercount].sin_family=AF_INET;
					serveraddress[servercount].sin_port=htons(port);
					servercount++;
				}
			}
		}

		fclose(fp);
	}


	numchunks=strtol(argv[2],0,10);
	if(numchunks>servercount)numchunks=servercount;

	printf("using %d chunks\n",numchunks);

	{
		//char filenameout[FILENAME_MAX]="out.zip";
		char filename[FILENAME_MAX];

		printf("enter filename\n");
		scanf("%s",filename);
		long long filesize=getfilesize(filename);

		if(filesize!=-1)
		{
			pthread_mutex_t mutex={};
			pthread_t *thd=0;
			FILE * fp=0;
			threadrequesttype *req=0;

			if(0!=pthread_mutex_init(&mutex,0))
					err_quit("Bad pthread_mutex_init\n");

			thd=(pthread_t*)malloc(sizeof(pthread_t)*numchunks);

			if(0==thd)
				err_quit("Bad malloc\n");

			req=(threadrequesttype*)malloc(sizeof(threadrequesttype)*numchunks);

			if(0==req)
				err_quit("Bad malloc\n");

			//fp=fopen(filenameout,"wb");
			fp=fopen(filename,"wb");

			if(0==req)
				err_quit("Bad fopen\n");


			{
				int allready=0;
				int i=0;
				long long chunksize=filesize/numchunks;
				///chunk size, last chunk may be a little bit bigger numchunks-1 bytes max

				for(i=0;i<numchunks;i++)
				{
					req[i].ready=0;
					req[i].filename=filename;
					req[i].fp=fp;
					req[i].pmutex=&mutex;
					req[i].startoffset=i*chunksize;

					req[i].serverindex=i;

					if(i==numchunks-1)
					req[i].transfersize=filesize-req[i].startoffset;
					else
					req[i].transfersize=chunksize;
				}

				for(i=0;i<numchunks;i++)
				pthread_create(&thd[i], 0,chunkdownloader, (void*)&req[i]);

				for(i=0;i<numchunks;i++)
				pthread_join(thd[i],0);


				allready=1;
				for(i=0;i<numchunks;i++)
				{
					if(!req[i].ready)allready=0;
				}

				if(!allready)
				{
					printf("not all chunks are downloaded\n");
				}
			}



			fclose(fp);
			free(req);
			free(thd);

			if(0!=pthread_mutex_destroy(&mutex))
					err_quit("Bad pthread_mutex_destroy\n");

		}
		else
		{
			printf("Cannot transfer this file\n");
		}

	}

    return 0;
}



///
int handshake(
int sockfd,
PACKET_HEADER *sendheader,const char * sendtext,int sendsize,
PACKET_HEADER *recvheader,char * recvtext,int recvmaxsize)
{
	sendheader->textlength=sendsize;

	if(-1==sendAll(sockfd,(char*)sendheader,sizeof(PACKET_HEADER)))
	{
		return -1;
	}
	if(-1==sendAll(sockfd,sendtext,sendheader->textlength))
	{
		return -1;
	}

	if(-1==recvAll(sockfd,(char*)recvheader,sizeof(PACKET_HEADER)))
	{
		return -1;
	}

	if(recvheader->textlength>recvmaxsize-1)
	{
		return -1;
	}


	if(-1==recvAll(sockfd,(char*)recvtext,recvheader->textlength))
	{
		return -1;
	}

	recvtext[recvheader->textlength]=0;

	return 0;
}


long long getfilechunkfromserver(threadrequesttype* req,int serverindex)
{
	int sockfd=Socket( AF_INET, SOCK_STREAM, 0);

	printf("getfilechunkfromserver %d \n",serverindex);

	///errors here are communication errors, should not kill the program, just fail the server
	if(-1==connect( sockfd,(struct sockaddr*) &serveraddress[serverindex],sizeof(serveraddress[serverindex])))
	{
		printf("Cannot connect\n");
		Close(sockfd);
		return -1;
	}


	{
		PACKET_HEADER response={};
		PACKET_HEADER request={};
		char chunk[4096];

		request.packettype=1;
		request.startoffset=req->startoffset;
		request.transfersize=req->transfersize;

		if(-1==handshake(sockfd,&request,req->filename,strlen(req->filename),&response,chunk,sizeof(chunk)))
		{
			printf("Cannot communicate\n");
			Close(sockfd);
			return -1;
		}


		if(response.textlength!=0)
		{
			printf(" %s\n",chunk);
		}

		if(response.packettype!=1)
		{
			printf("bad file here\n");
			Close(sockfd);
			return -1;
		}

		printf("good file found %lld + %lld\n",response.startoffset,response.transfersize);

		{
			long long pos=response.startoffset;
			long long size=response.transfersize;

			while(size)
			{
				long long chunksize=size;
				int res;
				if(chunksize>sizeof(chunk))chunksize=sizeof(chunk);


				res=recvAny(sockfd,chunk,chunksize);

				if(res<=0)
				{
					printf("disconnect\n");
					Close(sockfd);
					return -1;
				}

				{///errors here are fatal errors, should not happen

					if(0!=pthread_mutex_lock(req->pmutex))
						err_quit("Bad pthread_mutex_lock\n");

					if(0!=fseek(req->fp,pos,SEEK_SET))
						err_quit("Bad fseek\n");

					if(res!=fwrite(chunk,1,res,req->fp))
						err_quit("Bad fwrite\n");

					if(0!=pthread_mutex_unlock(req->pmutex))
						err_quit("Bad pthread_mutex_unlock\n");
				}
				pos+=res;
				size-=res;
			}
		}


		printf("recv ok\n");

		Close(sockfd);
		req->ready=1;
		return response.transfersize;
	}
}



long long getfilechunk(threadrequesttype* req)
{
	int i;

	for( i=0;i<servercount;i++)
	{
		long long res=getfilechunkfromserver(req,(req->serverindex+i)%servercount);

		if(res!=-1)return res;///file available on this server
	}

	return -1;
}

long long getfilesizefromserver(const char * filename,int serverindex)
{
	PACKET_HEADER response={};
	PACKET_HEADER request={};
	char chunk[4096];
	int sockfd=Socket( AF_INET, SOCK_STREAM, 0);

	printf("getfilesizefromserver %d \n",serverindex);

	if(-1==connect( sockfd,(struct sockaddr*) &serveraddress[serverindex],sizeof(serveraddress[serverindex])))
	{
		printf("cannot connect\n");
		Close(sockfd);
		return -1;
	}



	request.packettype=0;


	if(-1==handshake(sockfd,&request,filename,strlen(filename),&response,chunk,sizeof(chunk)))
	{
		printf("bad handshake\n");

		Close(sockfd);
		return -1;
	}


	if(response.textlength!=0)
	{
		printf(" %s\n",chunk);
	}

	if(response.packettype!=0)
	{
		printf("bad file here\n");

		Close(sockfd);
		return -1;
	}

	printf("good file found %lld\n",response.transfersize);

	Close(sockfd);
	return response.transfersize;
}


long long getfilesize(const char * filename)
{
	int i;

	for( i=0;i<servercount;i++)
	{
		long long res=getfilesizefromserver(filename,i);

		if(res!=-1)return res;///file available on this server
	}

	return -1;
}
