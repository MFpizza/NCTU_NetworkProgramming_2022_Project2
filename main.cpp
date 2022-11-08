#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <errno.h>
#include <iostream>
#include "npshell.h"
using namespace std;
#define LISTEN_BACKLOG 50

int passiveTCP(int port){
	int sockfd;
	struct sockaddr_in serv_addr;

	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		perror("S/Socket");
	
	memset((char*)&serv_addr, 0, sizeof(serv_addr));
	//bzero((char* )&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family		= AF_INET;
	serv_addr.sin_addr.s_addr	= htonl(INADDR_ANY);
	serv_addr.sin_port			= htons(port);

    int optval = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int)) == -1)
        perror("S/Setsocket");

	if(bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
		perror("S/Bind");
	
	if(listen(sockfd, LISTEN_BACKLOG)<0)
        perror("S/Listen");

	return sockfd;
}

int main(int argc,char* argv[]){
    int port = (argc>1)? atoi(argv[1]):7000;
    cout<<"[Port]: "<<port<<endl;
    int msock = passiveTCP(port); // master socket fd
    setenv("PATH", "bin:.", 1);

    struct sockaddr_in child_addr;	
	int addrlen = sizeof(child_addr);
    while(true){
        cout<<"wait for accept"<<endl;
        int ssock = accept(msock,(struct sockaddr*)&child_addr,(socklen_t*)&addrlen); // slave socket fd
        if(ssock<0)
            perror("S/Accept");

        int pid = fork();
        while(pid < 0){
            usleep(500);
            pid = fork();
        }
        if(pid>0)
            close(ssock);
        else{
            dup2(ssock,0);
			dup2(ssock,1);
			dup2(ssock,2);
            close(ssock);
			close(msock);
            shell();
        }
    }
}