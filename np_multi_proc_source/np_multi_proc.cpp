#include "np_multi_proc.h"
#include "np_multi_slave.h"
#include <arpa/inet.h>
#include <sys/mman.h>
#include <dirent.h>

const char welcome[] =
    "****************************************\n\
** Welcome to the information server. **\n\
****************************************\n";
client *clients;
broadcastMsg *BM;
string csfile;
string bmfile;

void logoutControl(int pid)
{
    // cout << "[Server] clients ptr:" << clients << endl;
    int fd, closeClientIndex;
    for (int index = 1; index < MAX_CLIENT; index++)
    {
        if (clients[index].used && clients[index].pid == pid)
        {
            printClients(index);
            closeClientIndex = index;
            close(clients[index].fd);
            clients[index].used = false;
            string output = "*** User \'" + string(clients[index].name) + "\' left. ***\n";
            for (int index = 1; index < MAX_CLIENT; index++)
                if (clients[index].used)
                    if (send(clients[index].fd, output.c_str(), output.size(), 0) < 0)
                        perror("broadcast/send");
            break;
        }
    }
    // clean up fifo
    for (int i = 1; i < MAX_CLIENT; i++)
    {
        string file1 = USERPIPE_PATH + to_string(30 * i + closeClientIndex);
        if ((unlink(file1.c_str()) < 0) && (errno != ENOENT))
            perror("unlink");
        string file2 = USERPIPE_PATH + to_string(30 * closeClientIndex + i);
        if ((unlink(file2.c_str()) < 0) && (errno != ENOENT))
            perror("unlink");
    }
}

void ServerBroadcast()
{
    for (int i = 1; i < MAX_BROADCAST; i++)
    {
        if (BM[i].used)
        {
            if (BM[i].toFD == clients[0].fd)
            {
                string output(BM[i].msg);
                cout << output << endl;
            }
            else if (BM[i].toFD != 0)
            {
                string output(BM[i].msg);
                if (send(BM[i].toFD, output.c_str(), output.size(), 0) < 0)
                    perror("broadcast/send");
            }
            else
            {
                string output(BM[i].msg);
                for (int index = 1; index < MAX_CLIENT; index++)
                    if (clients[index].used)
                        if (send(clients[index].fd, output.c_str(), output.size(), 0) < 0)
                            perror("broadcast/send");
            }
            BM[i].used = false;
        }
    }
}

void ServerSignalHandler(int sig)
{
    if (sig == SIGUSR1)
    {
        ServerBroadcast();
    }
    else if (sig == SIGCHLD)
    {
        int pid, status;
        pid = wait(&status);
        // cout << "[SigChld]: child pid " << pid << " exit" << endl;
        logoutControl(pid);
    }
    else if (sig == SIGINT)
    {
        cout << "\nserver SIGINT accept" << endl;

        for (int index = 1; index < MAX_CLIENT; index++)
        {
            if (clients[index].used)
            {
                cout << "[SigInt]: interrupt pid:" << clients[index].pid << endl;
                kill(clients[index].pid, SIGINT);
            }
        }

        if (munmap(clients, sizeof(client) * MAX_CLIENT) < 0)
            perror("munmap clients");
        if (remove(csfile.c_str()) < 0)
            perror("rm cs");

        if (munmap(BM, sizeof(broadcastMsg)) < 0)
            perror("munmap BM");
        if (remove(bmfile.c_str()) < 0)
            perror("rm bs");

        if (rmdir(SM_PATH.c_str()) < 0)
            perror("rmdir");

        for (int i = 1; i < MAX_CLIENT; i++)
        {
            for (int j = 1; j < MAX_CLIENT; j++)
            {
                string fifoFileName = "./user_pipe/" + to_string(30 * (i) + j);
                if ((unlink(fifoFileName.c_str()) < 0) && (errno != ENOENT))
                    perror("unlink");
            }
        }

        if (rmdir(USERPIPE_PATH.c_str()) < 0)
            perror("rmdir");
        exit(0);
    }
}

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

    int ppid = getpid();
    cout << "[Server pid]:" << ppid << endl;

    clients[0].pid = ppid;
    clients[0].used = true;
    strcpy(clients[0].name, "server");
    clients[0].fd = sockfd;
    return sockfd;
}

void newClientHandler(int fd, string name, string ip, int pid)
{
    for (int i = 1; i < MAX_CLIENT; i++)
    {
        if (!clients[i].used)
        {
            clients[i].fd = fd;
            strcpy(clients[i].ip, ip.c_str());
            strcpy(clients[i].name, name.c_str());
            clients[i].pid = pid;
            clients[i].used = true;
            printClients(i);
            return;
        }
    }
}

void printClients(int h)
{
    // printf("clients[%d]: \n", h);
    // printf("\t used:\t%d\n", clients[h].used);
    // printf("\t fd:\t%d\n", clients[h].fd);
    // printf("\t pid:\t%d\n", clients[h].pid);
    // printf("\t name:\t%s\n", clients[h].name);
}

void initMMap()
{
    if (NULL == opendir(SM_PATH.c_str()))
        mkdir(SM_PATH.c_str(), 0777);
    csfile = SM_PATH + "clientsSharedMemory";
    int clients_shared_momory_fd = open(csfile.c_str(), O_CREAT | O_RDWR, 00777);
    ftruncate(clients_shared_momory_fd, sizeof(client) * MAX_CLIENT);
    clients = (client *)mmap(NULL, sizeof(client) * MAX_CLIENT, PROT_READ | PROT_WRITE, MAP_SHARED, clients_shared_momory_fd, 0);
    if (clients == MAP_FAILED)
        perror("mmap");

    if (NULL == opendir(USERPIPE_PATH.c_str()))
        mkdir(USERPIPE_PATH.c_str(), 0777);
    bmfile = SM_PATH + "broadcastSharedMemory";
    int broadcast_shared_momory_fd = open(bmfile.c_str(), O_CREAT | O_RDWR, 00777);
    ftruncate(broadcast_shared_momory_fd, sizeof(broadcastMsg) * MAX_BROADCAST);
    BM = (broadcastMsg *)mmap(NULL, sizeof(broadcastMsg) * MAX_BROADCAST, PROT_READ | PROT_WRITE, MAP_SHARED, broadcast_shared_momory_fd, 0);
    if (BM == MAP_FAILED)
        perror("mmap");
}

int main(int argc, char *argv[])
{
    signal(SIGCHLD, ServerSignalHandler);
    signal(SIGINT, ServerSignalHandler);
    signal(SIGUSR1, ServerSignalHandler);

    initMMap();
    int port = (argc > 1) ? atoi(argv[1]) : 7000;
    cout << "[Port]: " << port << endl;

    struct sockaddr_in fsin;
    int msock = passiveTCP(port);

    while (true)
    {
        int alen = sizeof(fsin);
        int ssock = accept(msock, (struct sockaddr *)&fsin, (socklen_t *)&alen);
        if (ssock < 0)
        {
            perror("accept");
            continue;
        }

        string ip = inet_ntoa(fsin.sin_addr);
        ip = ip + ":" + to_string(ntohs(fsin.sin_port));
        int pid = fork();
        while (pid < 0)
        {
            perror("fork");
            usleep(500);
            pid = fork();
        }
        if (pid > 0)
        {
            newClientHandler(ssock, "(no name)", ip, pid);
        }
        else
        {
            send(ssock, welcome, sizeof(welcome) - 1, 0);
            dup2(ssock, 0);
            dup2(ssock, 1);
            dup2(ssock, 2);
            shellwithFD(ssock);
        }
    }
}
