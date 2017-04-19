
/* File Name: myserver.c
Name :     Mohammad Mirabian
Assignment:Homework 4
CruzID:    mmirabia
*/

#include "myunp.h"
#include <netdb.h>
#include <time.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

//list of forbidden websites
struct forbiddetempist {
    char host[128];
    struct forbiddetempist *next;
};


//prototypes
void sigHandler(int signo);
void handleClient(int c);
struct forbiddetempist *forbidHost = NULL;
void load_forbidden();
int checkForbidden(const char host[]);

static int serverSocket; 
FILE *logs;  
void write_log(const char str[]);

int main(int argc, char **argv) {
    int yes = 1;
    struct sockaddr_in servaddr, client;

	//use the default port number if not specified
    short port = 28888;
    if (argc == 2) {
        port = atoi(argv[1]);
    }


    if ((logs = fopen("log.txt", "w")) == NULL) {
        printf("Error: could not create log file.\n");
        exit(-1);
    }

    /* load the forbidden sites */
    load_forbidden();

    signal(SIGCHLD, sigHandler);
    signal(SIGINT, sigHandler);

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(port);

    serverSocket = Socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    Bind(serverSocket, (struct sockaddr*)&servaddr, sizeof(servaddr));
    Listen(serverSocket, 5);

    while (1) {
        socklen_t slen = sizeof(client);
        int c = Accept(serverSocket, (struct sockaddr*)&client, &slen);
        int child = fork();
        if (child < 0) {
            printf("Error: could not create new process to handle client.\n");
        } else if (child == 0) {
            handleClient(c);
            Close(c);
            exit(0);
        }
        Close(c);
    }

    return 0;
}

void sigHandler(int signo) {

	if(signo == SIGINT){
		    Close(serverSocket);
            fclose(logs);
            exit(0);
	}
	else if(signo == SIGCHLD){
		    while (waitpid(-1, NULL, WNOHANG) >= 0);
	}
}

void handleClient(int c) {
    char buff[512];
    int n;
    char method[256], uri[256], protocol[256], reserved[256], host[256];
    short port;
    char *temp;
    int httpSocket; 
    struct sockaddr_in servaddr;
    struct hostent *ent;
    int bytes;

    n = read(c, buff, sizeof(buff) - 1);
    if (n < 0) {
        printf("Could not read from the client.\n");
        return;
    }
    buff[n] = '\0';
    
	//parse the request
    if (sscanf(buff, "%s %s %s", method, uri, protocol) != 3) {
        write(c, "HTTP/1.1 ", 9);
		write(c, "400 Bad Request\n", 16);
        return;
    }
    if ((strcmp(method, "GET") != 0 && strcmp(method, "HEAD") != 0) ||
        (strcmp(protocol, "HTTP/1.0") != 0 && strcmp(protocol, "HTTP/1.1")) != 0) {
		write(c, "HTTP/1.1 ", 9);
		write(c, "501 Not implemented\n", 20);
		return;
	}

	//we need to get the host address 
    temp = strchr(buff, '\n');
    if (temp != NULL) temp ++;
    while (temp != NULL && strncmp(temp, "Host", 4) != 0) {
        temp = strchr(temp, '\n');
        if (temp != NULL) temp ++;
    }
    if (temp == NULL || sscanf(temp, "%s %s", reserved, host) != 2) {
        write(c, "HTTP/1.1 ", 9);
		write(c, "400 Bad Request\n", 16);
        return;
    }

    temp = strchr(host, ':');
    if (temp != NULL) {
        *temp = 0;
        port = atoi(temp + 1);
    } else {
        port = 80; //port 80 for http
    }
    if (checkForbidden(host)) {
        write(c, "HTTP/1.1 ", 9);
		write(c, "403 Forbidden URL\n", 18);

        sprintf(reserved, "%s %s host=%s uri=%s action=filtered",
            method, protocol, host, uri);
        write_log(reserved);
        return;
    }



    if ((ent = gethostbyname(host)) == NULL) {
        write(c, "HTTP/1.1 ", 9);
		write(c, "404 Not Found\n", 14);
        sprintf(reserved, "%s %s host=%s uri=%s error=nohost",
            method, protocol, host, uri);
        write_log(reserved);
        return;
    }


    httpSocket = Socket(AF_INET, SOCK_STREAM, 0);
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    memcpy(&servaddr.sin_addr, ent->h_addr_list[0], ent->h_length);
    servaddr.sin_port = htons(port);

    if (connect(httpSocket, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
        write(c, "HTTP/1.1 ", 9);
		write(c, "404 Not Found\n", 14);
        return;
    }

    if (write(httpSocket, buff, n) < 0) { 
        write(c, "HTTP/1.1 ", 9);
		write(c, "400 Bad Request\n", 16);
        return;
    }

    sprintf(reserved, "%s %s host=%s uri=%s action=forwarded",
            method, protocol, host, uri);
    write_log(reserved);


    while ((bytes = read(httpSocket, buff, sizeof(buff))) > 0) {
        buff[bytes] = 0;
        if (write(c, buff, bytes) < 0) { 
            break;
        }
    }
    close(httpSocket);
}


void load_forbidden() {
    char forbiddensites[128];
    FILE *sitesptr = fopen("forbidden-sites", "r");
    if (sitesptr == NULL) {
        return;
    }
  
    while (fgets(forbiddensites, sizeof(forbiddensites), sitesptr) != NULL) {
        struct forbiddetempist *host;
        char *pos = strchr(forbiddensites, '\n');
        if (pos != NULL) *pos = '\0';
        host = (struct forbiddetempist *)malloc(sizeof(struct forbiddetempist));
        host->next = forbidHost;
        strcpy(host->host, forbiddensites);
        forbidHost = host;
    }
    fclose(sitesptr);
}


int checkForbidden(const char site[]) {
    struct forbiddetempist *host = forbidHost;
    while (host != NULL) {
        if (strcasecmp(host->host, site) == 0) {
            return 1;
        }
        host = host->next;
    }
    return 0;
}



void write_log(const char str[]) {
    time_t thistime;
    struct tm  *timeinfo;
    char info[128];
    time(&thistime);
    timeinfo = localtime(&thistime);
    strftime(info, sizeof(info)-1, "[%m/%d/%y %H:%M:%S] ", timeinfo);
    strcat(info, str);
    fprintf(logs, "%s\n", info);
    fflush(logs);
    printf("%s\n", info);
}

