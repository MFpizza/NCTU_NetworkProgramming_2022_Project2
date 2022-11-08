
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

int main(int argc, char *argv[])
{
    setenv("PATH", "bin:.", 1);

    int port = (argc > 1) ? atoi(argv[1]) : 7000;
    cout << "[Port]: " << port << endl;
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

        if (select(nfds, &rfds, (fd_set *)0, (fd_set *)0, (struct timeval *)0) < 0)
            perror("select");

        if (FD_ISSET(msock, &rfds))
        {
            int ssock;
            alen = sizeof(fsin);
            ssock = accept(msock, (struct sockaddr *)&fsin, (socklen_t *)&alen);
            if (ssock < 0)
                perror("accept");
            FD_SET(ssock, &afds);
            char colon[] = ":";
            string ip = inet_ntoa(fsin.sin_addr);
            ip = ip + ":" + to_string(ntohs(fsin.sin_port));
            client nC = {clients.size() + 1, ssock, "(no name)", ip};
            clients.push_back(nC);

            send(ssock, welcome, sizeof(welcome), 0);

            broadcast(LOGIN, nC, "", 0);

            send(ssock, "% ", 2, 0);
        }
        for (fd = 0; fd < nfds; fd++)
        {
            if (fd != msock && FD_ISSET(fd, &rfds))
            {
                if (shellwithFD(fd) < 1)
                {
                    client c;
                    for (int clientIndex = 0; clientIndex < clients.size(); clientIndex++)
                    {
                        if (clients[clientIndex].fd == fd)
                        {
                            c = clients[clientIndex];
                            break;
                        }
                    }
                    broadcast(LOGOUT, c, "", 0);
                    close(fd);
                    // cout << "fd:" << fd << " is closed" << endl;
                    FD_CLR(fd, &afds);
                }
            }
        }
    }
}