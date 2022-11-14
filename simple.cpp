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

int passiveTCP(int port)
{
    int sockfd;
    struct sockaddr_in serv_addr;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        perror("S/Socket");

    memset((char *)&serv_addr, 0, sizeof(serv_addr));
    // bzero((char* )&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);

    int optval = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int)) == -1)
        perror("S/Setsocket");

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        perror("S/Bind");

    if (listen(sockfd, LISTEN_BACKLOG) < 0)
        perror("S/Listen");

    return sockfd;
}

void ServersignalHandler(int sig)
{
    pid_t pid = wait(NULL);
}

int main(int argc, char *argv[])
{
    // signal(SIGCHLD, ServersignalHandler);
    int port = (argc > 1) ? atoi(argv[1]) : 7000;
    cout << "[Port]: " << port << endl;
    int msock = passiveTCP(port); // master socket fd
    int ssock;                    // slave socket fd
    setenv("PATH", "bin:.", 1);

    struct sockaddr_in child_addr;
    int addrlen = sizeof(child_addr);
    int std[3];
    while (true)
    {
        cout << "wait for accept" << endl;
        ssock = accept(msock, (struct sockaddr *)&child_addr, (socklen_t *)&addrlen); // slave socket fd
        if (ssock < 0)
            perror("S/Accept");
        cout << "ssock fd:" << ssock << endl;

        for (int i = 0; i < 4; i++){
            std[i] = dup(i);
            dup2(ssock, i);
        }
        shell();

        close(ssock);

        for (int i = 0; i < 4; i++){
            dup2(std[i], i);
            close(std[i]);
        }
        cout<<"loop end"<<endl;
    }
}