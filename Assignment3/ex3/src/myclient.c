
/* File Name: myclient.c
Name :     Mohammad Mirabian
Assignment:Homework 3
CruzID:    mmirabia
*/

#include "myunp.h"
#include "errno.h"
#include <time.h>
#include <pthread.h>


#define MAX_RETRY 20
#define RETRY_TIMEOUT 0.1
#define MAX_SERVERS 20
#define MAXIMUM_DATA 1024

typedef struct
{
	unsigned int type;
	unsigned int seqnumber;
	long long offset;
	long long chunksize;
}UDP_HEADER;


typedef struct
{
	UDP_HEADER header;
	char data[MAXIMUM_DATA];

}UDP_PACKET;




struct sockaddr_in servaddr[MAX_SERVERS];
int servercount=0;

int exchangePackets(
int sockfd,
struct sockaddr_in *destaddr,
const UDP_PACKET *request,int sendsize,
UDP_PACKET *response,int maxretry,double maxtimeout)
{
	int retry;


	for(retry=0;retry<maxretry;retry++)
	{
		int res;
		time_t startTime, endTime;
		double timeDifference;

		time(&startTime);


		sendto(sockfd,(const void*)request,sendsize,0,(struct sockaddr*)destaddr,sizeof(struct sockaddr_in));


		for(;;)
		{
			struct sockaddr_in recvaddr;
			socklen_t recvaddrlen=sizeof(recvaddr);
			fd_set fdset;
			struct  timeval tm={};
			FD_ZERO(&fdset);
			FD_SET(sockfd,&fdset);

			time(&endTime);
			timeDifference = difftime(endTime, startTime);

			timeDifference=maxtimeout-timeDifference;

			if(timeDifference+maxtimeout<=0)break;

			tm.tv_sec=timeDifference;
			tm.tv_usec=(timeDifference-tm.tv_sec)*1000000;


			if(0>=select(1+sockfd,&fdset,0,0,&tm))
			{
				break;///error
			}


			if(!FD_ISSET(sockfd,&fdset))
			{
				break;
			}

			res=recvfrom(sockfd,(void*)response,sizeof(UDP_PACKET),0,(struct sockaddr*)&recvaddr,&recvaddrlen);

			if(res<=0)
			{
				break;
			}

			if(recvaddrlen!=sizeof(recvaddr))
			{
				continue;
			}

			if(
			(destaddr->sin_addr.s_addr!=recvaddr.sin_addr.s_addr)||
			(destaddr->sin_port!=recvaddr.sin_port))
			{
				continue;
			}

			if(res<sizeof(response->header))
			{
				continue;
			}

			if(request->header.seqnumber==(response->header.seqnumber))
			{
				return res;
			}

		}

	}

	return -1;
}


typedef struct
{
int index;
long long offset;
long long chunksize;
FILE * fp;
pthread_mutex_t *pmutex;
char *filename;
int success;
}threadrequesttype;


int getfilechunk(threadrequesttype* req);
long long getfilesize(const char * filename);



void * chunkdownloader(void *arg)
{
	getfilechunk((threadrequesttype*)arg);

	return 0;
}

void readserverlist(const char * filename)
{
	char fileline[1024];
	FILE * fp=fopen(filename,"rb");
	if(!fp)
	{
		err_quit("Cannot open %s", filename);
	}

	while(fgets(fileline,sizeof(fileline),fp)!=0)
	{
		char ip[1024];
		int port=0;

		if(sscanf(fileline," %s %d",ip,&port)!=2)
			err_quit("Incorrect file\n");


		if(servercount<MAX_SERVERS)
		{
			if(!strcmp("localhost",ip))
			{
				servaddr[servercount].sin_addr.s_addr=htonl(INADDR_LOOPBACK);
				servaddr[servercount].sin_family=AF_INET;
				servaddr[servercount].sin_port=htons(port);
				servercount++;
			}
			else
			if(0!= inet_aton(ip, &servaddr[servercount].sin_addr))///good address
			{
				servaddr[servercount].sin_family=AF_INET;
				servaddr[servercount].sin_port=htons(port);
				servercount++;
			}
		}
	}
	fclose(fp);
}

int main(int argc, char **argv)
{
	int numchunks=0;

	if(argc<3)
	{
		err_quit("format is myclient <server-info.text> <num-connections>\n");
	}

	readserverlist(argv[1]);

	numchunks=strtol(argv[2],0,10);

	if(numchunks>servercount)numchunks=servercount;

	printf("Transfer using %d chunks\n",numchunks);

	{
		char filename[FILENAME_MAX];
		char transferedName[FILENAME_MAX];

		printf("Type in the name of the file to be transfered: ");
		scanf("%s",filename);
		long long filesize=getfilesize(filename);


		strcpy(transferedName,"downloaded_");
		strcat(transferedName,filename);

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
				err_quit("malloc error\n");

			fp=fopen(transferedName,"wb");

			if(0==req)
				err_quit("fopen error\n");

			{
				int allsuccess=0;
				int i=0;
				
				long long chunksize=filesize/numchunks;

				
				for(i=0;i<numchunks;i++)
				{
					req[i].success=0;
					req[i].filename=filename;
					req[i].fp=fp;
					req[i].pmutex=&mutex;
					req[i].offset=i*chunksize;
					req[i].index=i;

					if(i==numchunks-1)
					req[i].chunksize=filesize-req[i].offset;
					else
					req[i].chunksize=chunksize;
				}

				for(i=0;i<numchunks;i++)
				pthread_create(&thd[i], 0,chunkdownloader, (void*)&req[i]);

				for(i=0;i<numchunks;i++)
				pthread_join(thd[i],0);


				allsuccess=1;
				for(i=0;i<numchunks;i++)
				{
					if(!req[i].success)allsuccess=0;
				}

				if(!allsuccess)
				{
					printf("Download failed\n");
				}
				else
				{
					printf("Successfull download of %s\n",transferedName);
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
			printf("File could not be transferd\n");
		}

	}

    return 0;
}



int getfilechunkfromserver(threadrequesttype* req,int index)
{
	int sockfd;

	if(strlen(req->filename)>=MAXIMUM_DATA)
	{
		err_quit("Too long filename !\n");
	}

	sockfd=Socket( AF_INET, SOCK_DGRAM, 0);


	for(;req->chunksize>0;)
	{
		UDP_PACKET response={};
		UDP_PACKET request={};
		int res;

		request.header.type=3;
		request.header.seqnumber=rand();
		request.header.offset=req->offset;
		request.header.chunksize=req->chunksize;

		if(request.header.chunksize>MAXIMUM_DATA)
			request.header.chunksize=MAXIMUM_DATA;

		strcpy(request.data,req->filename);

		res=exchangePackets(sockfd,
		&servaddr[index],
		&request,sizeof(request.header)+strlen(req->filename),
		&response,MAX_RETRY,RETRY_TIMEOUT);


		if(res<=0)
		{
			printf("Cannot communicate\n");
			Close(sockfd);
			return -1;
		}

		if(res<sizeof(response.header))
		{
			printf("Cannot communicate\n");
			Close(sockfd);
			return -1;
		}

		if((response.header.type==0)&&(res>sizeof(response.header)))
		{
			if(res>MAXIMUM_DATA-1+sizeof(response.header))
			res=MAXIMUM_DATA-1+sizeof(response.header);

			response.data[res-sizeof(response.header)]=0;
			printf("%s\n",response.data);
			Close(sockfd);
			return -1;
		}

		if(response.header.type!=4)
		{
			printf("Error response\n");
			Close(sockfd);
			return -1;
		}

		if(res-sizeof(response.header)!=response.header.chunksize)
		{
			printf("Error response size\n");
			Close(sockfd);
			return -1;
		}

		{

			{

				if(0!=pthread_mutex_lock(req->pmutex))
					err_quit("Error pthread_mutex_lock\n");

				if(0!=fseek(req->fp,response.header.offset,SEEK_SET))
					err_quit("Error fseek\n");

				if(response.header.chunksize!=fwrite(response.data,1,response.header.chunksize,req->fp))
					err_quit("Error fwrite\n");

				if(0!=pthread_mutex_unlock(req->pmutex))
					err_quit("Error pthread_mutex_unlock\n");
			}

			req->offset+=response.header.chunksize;
			req->chunksize-=response.header.chunksize;
		}
	}

	Close(sockfd);
	req->success=1;
	return 0;
}



int getfilechunk(threadrequesttype* req)
{
	int i;

	for( i=0;i<servercount;i++)
	{
		int res;

		printf("Try  get file chunk from server %d \n",(req->index+i)%servercount);

		res=getfilechunkfromserver(req,(req->index+i)%servercount);

		if(res!=-1)
		{
			printf("success\n");
			return res;
		}

		printf("Failed %d\n",(req->index+i)%servercount);
	}

	return -1;
}

long long getfilesizefromserver(const char * filename,int index)
{
	int sockfd;

	UDP_PACKET response={};
	UDP_PACKET request={};
	int res;


	if(strlen(filename)>=MAXIMUM_DATA)
	{
		err_quit("too long filename !\n");
	}

	sockfd=Socket( AF_INET, SOCK_DGRAM, 0);

	request.header.type=1;
	request.header.seqnumber=rand();
	request.header.offset=0;
	request.header.chunksize=0;

	strcpy(request.data,filename);

	res=exchangePackets(sockfd,
	&servaddr[index],
	&request,sizeof(request.header)+strlen(filename),
	&response,MAX_RETRY,RETRY_TIMEOUT);

	if(res<=0)
	{
		printf("Cannot communicate\n");
		Close(sockfd);
		return -1;
	}

	if(res<sizeof(response.header))
	{
		printf("Cannot communicate\n");
		Close(sockfd);
		return -1;
	}

	if((response.header.type==0)&&(res>sizeof(response.header)))
	{
		if(res>MAXIMUM_DATA-1+sizeof(response.header))
		res=MAXIMUM_DATA-1+sizeof(response.header);

		response.data[res-sizeof(response.header)]=0;///ensure null terminate
		printf("%s\n",response.data);
		Close(sockfd);
		return -1;
	}

	if(response.header.type!=2)
	{
		printf("Error in type\n");
		Close(sockfd);
		return -1;
	}

	Close(sockfd);

	return response.header.chunksize;
}


long long getfilesize(const char * filename)
{
	int i;

	for( i=0;i<servercount;i++)
	{
		long long res;

		printf("Try  get file size from server %d \n",i);

		res=getfilesizefromserver(filename,i);

		if(res!=-1)
		{
			printf("Success\n");
			return res;
		}

		printf("Failed %d\n",i);
	}

	return -1;
}
