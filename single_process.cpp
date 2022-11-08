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
#define QLEN 5
#define BUFSIZE 4096

int passiveTCP(int port)
{
    int sockfd;
    struct sockaddr_in sin;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        perror("S/Socket");

    memset((char *)&sin, 0, sizeof(sin));
    // bzero((char* )&serv_addr, sizeof(serv_addr));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(port);

    int optval = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int)) == -1)
        perror("S/Setsocket");

    if (bind(sockfd, (struct sockaddr *)&sin, sizeof(sin)) < 0)
        perror("S/Bind");

    if (listen(sockfd, LISTEN_BACKLOG) < 0)
        perror("S/Listen");

    return sockfd;
}

int echo(int fd)
{
    char buf[BUFSIZE];
    int cc;
    cc = read(fd, buf, sizeof(buf));
    if (cc < 0)
        perror("read");
    if (cc && write(fd, buf, cc) < 0)
        perror("write");
    return cc;
}

int main(int argc, char *argv[])
{
    setenv("PATH", "bin:.", 1);

    int port = (argc > 1) ? atoi(argv[1]) : 7000;
    cout << "[Port]: " << port << endl;
    struct sockaddr_in fsin;
    int msock; // master socket fd
    fd_set rfds;
    fd_set afds;
    int alen;
    int fd, nfds;

    msock = passiveTCP(port);

    nfds = getdtablesize();
    FD_ZERO(&afds);
    FD_SET(msock, &afds);

    while (true)
    {
        memcpy(&rfds, &afds, sizeof(rfds));

        for(int i=0;i<nfds;i++){
             cout<<FD_ISSET(i, &afds);
             if(i==10)
                cout<<endl;
        }

        cout<<"wait for select"<<endl;
        if (select(nfds, &rfds, (fd_set *)0, (fd_set *)0, (struct timeval *)0) < 0)
            perror("select");

        cout << nfds << endl;
        if (FD_ISSET(msock, &rfds))
        {
            int ssock;
            alen = sizeof(fsin);
            cout<<"wait for accept"<<endl;
            ssock = accept(msock, (struct sockaddr *)&fsin, (socklen_t *)&alen);
            if (ssock < 0)
                perror("accept");
            FD_SET(ssock, &afds);
        }
        for (fd = 0; fd < nfds; fd++)
        {
            cout<<FD_ISSET(fd, &rfds)<<" "<<FD_ISSET(fd, &afds)<<endl;
            if (fd != msock && FD_ISSET(fd, &rfds))
            {
                cout<<fd<<endl;
                if (echo(fd) == 0)
                {
                    close(fd);
                    FD_CLR(fd, &afds);
                }
                sleep(3);
            }
        }
    }
}