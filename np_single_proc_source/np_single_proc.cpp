#include "npshellForSingleP.h"

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

void newClientHandler(int fd, string ip)
{
    for (int i = 1; i < clients.size(); i++)
    {
        if (clients[i] == NULL)
        {
            clients[i] = new client(fd, "(no name)", ip);
            setEnv("PATH", "bin:.", clients[i]);
            send(fd, welcome, sizeof(welcome) - 1, 0);
            broadcast(LOGIN, clients[i], "", 0, 0);
            send(fd, "% ", 2, 0);
            return;
        }
    }
    return;
}

void logoutControl(int fd)
{
    int clientIndex;
    for (int i = 1; i < clients.size(); i++)
    {
        if (clients[i] != NULL)
            if (clients[i]->fd == fd)
            {
                broadcast(LOGOUT, clients[i], "", 0, 0);
                delete clients[i];
                clients[i] = NULL;
                clientIndex = i;
                userPipe.erase(i);
            }
    }
    for (int i = 1; i < clients.size(); i++)
    {
        userPipe[i].erase(clientIndex);
    }
}

int main(int argc, char *argv[])
{
    setenv("PATH", "bin:.", 1);

    int port = (argc > 1) ? atoi(argv[1]) : 7000;
    cout << "[Port]: " << port << endl;
    clients.resize(31, NULL);
    struct sockaddr_in fsin;

    int alen;
    int fd;

    msock = passiveTCP(port);

    nfds = FD_SETSIZE;
    FD_ZERO(&afds);
    FD_SET(msock, &afds);

    while (true)
    {
        memcpy(&rfds, &afds, sizeof(rfds));

        int err, stat;
        do
        {
            stat = select(nfds, &rfds, (fd_set *)0, (fd_set *)0, (struct timeval *)0);
            if (stat < 0)
                err = errno;
        } while ((stat < 0) && (err == EINTR)); // 被signal跳出去了，要讓他重跑
        if (FD_ISSET(msock, &rfds))
        {
            int ssock;
            alen = sizeof(fsin);
            ssock = accept(msock, (struct sockaddr *)&fsin, (socklen_t *)&alen);
            if (ssock < 0)
                perror("accept");
            FD_SET(ssock, &afds);
            cout<<ssock<<endl;
            string ip = inet_ntoa(fsin.sin_addr);
            ip = ip + ":" + to_string(ntohs(fsin.sin_port));
            newClientHandler(ssock, ip);
        }
        for (fd = 0; fd < nfds; fd++)
            if (fd != msock && FD_ISSET(fd, &rfds))
            {
                int *std=new int[3];
                for(int i=0; i<3; i++)
                    std[i]=dup(i);
                if (shellwithFD(fd) < 1)
                {
                    logoutControl(fd);
                    close(fd);
                    FD_CLR(fd, &afds);
                }
                for(int i=0; i<3; i++){
                    close(i);
                    dup2(std[i],i);
                    close(std[i]);
                }
                delete[]std;
            }
    }
}