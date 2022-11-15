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

void newClientHandler(client *client)
{
    for (int i = 1; i < clients.size(); i++)
    {
        if (clients[i] == NULL)
        {
            clients[i] = client;
            return;
        }
    }
    clients.push_back(client);
    return;
}

int main(int argc, char *argv[])
{
    setenv("PATH", "bin:.", 1);

    int port = (argc > 1) ? atoi(argv[1]) : 7000;
    cout << "[Port]: " << port << endl;
    clients.resize(1);
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
            
            string ip = inet_ntoa(fsin.sin_addr);
            ip = ip + ":" + to_string(ntohs(fsin.sin_port));
            client *nC = new client(ssock, "(no name)", ip);
            setEnv("PATH", "bin:.", nC);
            newClientHandler(nC);
            send(ssock, welcome, sizeof(welcome) - 1, 0);
            broadcast(LOGIN, nC, "", 0, 0);
            send(ssock, "% ", 2, 0);
        }
        for (fd = 0; fd < nfds; fd++)
        {
            if (fd != msock && FD_ISSET(fd, &rfds))
            {
                if (shellwithFD(fd) < 1)
                {
                    client *c;
                    int clientIndex;
                    for (clientIndex = 1; clientIndex < clients.size(); clientIndex++)
                    {
                        if (clients[clientIndex] != NULL && clients[clientIndex]->fd == fd)
                        {
                            c = clients[clientIndex];
                            break;
                        }
                    }
                    broadcast(LOGOUT, c, "", 0, 0);
                    logoutControl(fd, clientIndex);
                    close(fd);
                    // cout << "fd:" << fd << " is closed" << endl;
                    FD_CLR(fd, &afds);
                }
            }
        }
    }
}