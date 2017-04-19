
#include "myunp.h"
#include "myprotocol.h"
#include <sys/stat.h>



///return 1 on sucessfull reception of all data
int recvAll(int sockfd, char *buffer, int bufferlen)
{
	while(bufferlen)
	{
		int res=recv(sockfd,buffer,bufferlen,0);

		if(res<=0)
		{
			printf("Error, could not receive\n");///non fatal error
			return -1;
		}

		bufferlen-=res;
		buffer+=res;
	}

	return 0;
}


///return data recv on sucessfull reception of any data
int recvAny(int sockfd, char *buffer, int bufferlen)
{
	int res=recv(sockfd,buffer,bufferlen,0);

	if(res<=0)
	{
		printf("Error, could not receive\n");///non fatal error
		return -1;
	}

	return res;
}




///return 1 on sucessfull reception of all data
int sendAll(int sockfd, const char *buffer, int bufferlen)
{
	while(bufferlen)
	{
		int res=send(sockfd,buffer,bufferlen,MSG_NOSIGNAL);///prevent stupid signals , we don't want to die

		if(res<=0)
		{
			printf("Error, could not send\n");///non fatal error, we handle it by just dropping the client
			return -1;
		}

		bufferlen-=res;
		buffer+=res;
	}

	return 0;
}



int sendErrorString(int sockfd,const char *text)
{
	PACKET_HEADER response={};

	response.packettype=-1;///error

	response.textlength=strlen(text);

	printf("Non fatal error: %s",text);

	if(-1==sendAll(sockfd,(char*)&response,sizeof(response)))return-1;

	if(-1==sendAll(sockfd,text,response.textlength))return-1;

	return 0;
}

