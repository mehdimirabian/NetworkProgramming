#ifndef MY_HELPERS_H
#define MY_HELPERS_H


typedef struct
{
long long startoffset;
long long transfersize;
unsigned int packettype;
unsigned int textlength;
}PACKET_HEADER;


int recvAll(int sockfd, char *buffer, int bufferlen);
int recvAny(int sockfd, char *buffer, int bufferlen);
int sendAll(int sockfd, const char *buffer, int bufferlen);
int sendErrorString(int sockfd,const char *text);


#endif
