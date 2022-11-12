#include "np_multi_proc.h"

void logoutControl(int pid)
{
    int fd, clientIndex;
    for (int index = 1; index < MAX_CLIENT; index++)
    {
        if (clients[index].pid == pid)
        {
            broadcast(LOGOUT, clients[index].fd, "", 0, 0);
            clients[index].used = false;
            close(clients[index].fd);
            break;
        }
    }
    // for (int i = 1; i < MAX_CLIENT; i++)
    // {
    //     if (clients[i].used)
    //         if (clients[i].fd == fd)
    //         {
    //             userPipe.erase(i);
    //         }
    //         else
    //         {
    //             userPipe[i].erase(clientIndex);
    //         }
    // }
}

// TODO: 創建一個signal handler 處理client離開的事情
void ServerSignalHandler(int sig)
{
    if (sig == SIGUSR1)
    {
        cout << "server SIGUSR1: ready to broadcast" << endl;
        ServerBroadcast();
    }
    else if (sig == SIGCHLD)
    {
        cout << "child exit" << endl;
        int pid, status;
        pid = wait(&status);
        logoutControl(pid);
    }
    else if (sig == SIGINT)
    {
        // TODO: clean shared memory and FIFO

        cout << "\nserver SIGINT accept" << endl;

        for (int index = 1; index < MAX_CLIENT; index++)
        {
            if (clients[index].used)
            {
                cout << clients[index].pid << endl;
                kill(clients[index].pid, SIGINT);
            }
        }

        if (munmap(clients, sizeof(client) * MAX_CLIENT) < 0)
            perror("munmap clients");
        if (remove("./shared_memory/clientsSharedMemory") < 0)
            perror("rm cs");

        if (munmap(BM, sizeof(broadcastMsg)) < 0)
            perror("munmap BM");
        if (remove("./shared_memory/broadcastSharedMemory") < 0)
            perror("rm bs");

        if (rmdir("./shared_memory/") < 0)
            perror("rmdir");
        exit(0);
    }
    else
    {
        cout << "signal:" << sig << endl;
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

void printClients(int i)
{
    for (int h = 0; h <= i; h++)
    {
        printf("clients[%d]: \n\t used:\t%d\n\t fd:\t%d\n\t pid:\t%d\n\t name:\t%s\n", h, clients[h].used, clients[h].fd, clients[h].pid, clients[h].name);
    }
}

void initMMap()
{
    clients_shared_momory_fd = open("./shared_memory/clientsSharedMemory", O_CREAT | O_RDWR, 00777);
    ftruncate(clients_shared_momory_fd, sizeof(client) * MAX_CLIENT);
    clients = (client *)mmap(NULL, sizeof(client) * MAX_CLIENT, PROT_READ | PROT_WRITE, MAP_SHARED, clients_shared_momory_fd, 0);
    if (clients == MAP_FAILED)
        perror("mmap");

    broadcast_shared_momory_fd = open("./shared_memory/broadcastSharedMemory", O_CREAT | O_RDWR, 00777);
    ftruncate(broadcast_shared_momory_fd, sizeof(broadcastMsg) * MAX_BROADCAST);
    BM = (broadcastMsg *)mmap(NULL, sizeof(broadcastMsg) * MAX_BROADCAST, PROT_READ | PROT_WRITE, MAP_SHARED, broadcast_shared_momory_fd, 0);
    if (BM == MAP_FAILED)
        perror("mmap");
}

void initFIFO()
{
    for (int i = 1; i < MAX_CLIENT; i++)
    {
        for (int j = 1; j < MAX_CLIENT; j++)
        {
            string fifoFileName = "./user_pipe/" + to_string(30 * (i) + j);
            if ((mknod(fifoFileName.c_str(), S_IFIFO, 0) < 0) && (errno != EEXIST))
                perror("mknod");
        }
    }
}

int main(int argc, char *argv[])
{
    signal(SIGCHLD, ServerSignalHandler);
    signal(SIGINT, ServerSignalHandler);
    signal(SIGUSR1, ServerSignalHandler);

    if (NULL == opendir(USERPIPE_PATH))
        mkdir(USERPIPE_PATH, 0777);
    if (NULL == opendir(SM_PATH))
        mkdir(SM_PATH, 0777);

    initMMap();
    initFIFO();
    int port = (argc > 1) ? atoi(argv[1]) : 7000;
    cout << "[Port]: " << port << endl;
    struct sockaddr_in fsin;

    int alen, pid;
    ppid = getpid();
    cout << "[Server pid]:" << ppid << endl;
    msock = passiveTCP(port);

    clients[0].pid = ppid;
    clients[0].used = true;
    strcpy(clients[0].name, "server");
    clients[0].fd = msock;

    while (true)
    {
        alen = sizeof(fsin);
        int ssock = accept(msock, (struct sockaddr *)&fsin, (socklen_t *)&alen);
        if (ssock < 0)
        {
            cout << "fuc" << endl;
            perror("accept");
            continue;
        }
        cout << ssock << endl;

        pid = fork();
        while (pid < 0)
        {
            usleep(500);
            pid = fork();
        }
        if (pid > 0)
        {
            string ip = inet_ntoa(fsin.sin_addr);
            ip = ip + ":" + to_string(ntohs(fsin.sin_port));
            newClientHandler(ssock, "(no name)", ip, pid);
            send(ssock, welcome, sizeof(welcome) - 1, 0);
            broadcast(LOGIN, ssock, "", 0, 0);
            // close(ssock);
            cout << "end" << endl;
        }
        else
        {
            dup2(ssock, 0);
            dup2(ssock, 1);
            dup2(ssock, 2);
            shellwithFD(ssock);
        }
    }
}

void broadcast(BROADCAST_TYPE type, int fromFD, string msg, int targetFD, int targetIndex)
{
    client *from = NULL;
    int indexFrom, indexTarget;
    for (int i = 1; i < MAX_CLIENT; i++)
    {
        if (!clients[i].used)
            continue;
        if (fromFD == clients[i].fd)
        {
            indexFrom = i;
            from = &clients[i];
        }
        if (clients[i].fd == targetFD)
            indexTarget = i;
    }

    broadcastMsg *bm;
    bool findBMEmpty = false;
    while (!findBMEmpty)
    {
        for (int i = 0; i < MAX_BROADCAST; i++)
        {
            if (!BM[i].used)
            {
                BM[i].used = true;
                findBMEmpty = true;
                bm = &BM[i];
            }
        }
    }

    char *buf = BM->msg;
    memset(buf, 0, sizeof(char) * BUFSIZE);

    switch (type)
    {
    case LOGIN:
        sprintf(buf, "*** User '%s' entered from %s. ***\n", from->name, from->ip);
        break;
    case LOGOUT:
        sprintf(buf, "*** User '%s' left. ***\n", from->name);
        break;
    case NAME:
        sprintf(buf, "*** User from %s is named '%s'. ***\n", from->ip, from->name);
        break;
    case YELL:
        sprintf(buf, "*** %s yelled ***: %s\n", from->name, msg.c_str());
        break;
    case TELL:
        sprintf(buf, "*** %s told you ***: %s\n", from->name, msg.c_str());
        break;
    case SEND:
        sprintf(buf, "*** %s (#%d) just piped '%s' to %s (#%d) ***\n", from->name, indexFrom, msg.c_str(), clients[indexTarget].name, indexTarget);
        break;
    case RECV:
        sprintf(buf, "*** %s (#%d) just received from %s (#%d) by '%s' ***\n", from->name, indexFrom, clients[indexTarget].name, indexTarget, msg.c_str());
        break;
    case ERROR_USER:
        sprintf(buf, "*** Error: user #%d does not exist yet. ***\n", targetIndex);
        break;
    case ERROR_PIPE_NOT_EXIST:
        sprintf(buf, "*** Error: the pipe #%d.#%d does not exist yet. ***\n", targetIndex, indexFrom);
        break;
    case ERROR_PIPE_IS_EXIST:
        sprintf(buf, "*** Error: the pipe #%d.#%d already exists. ***\n", indexFrom, targetIndex);
        break;
    default:
        break;
    }

    if (type == TELL)
    {
        BM->toFD = targetFD;
    }
    else if (type == ERROR_USER || type == ERROR_PIPE_NOT_EXIST || type == ERROR_PIPE_IS_EXIST)
    {
        BM->toFD = from->fd;
    }
    else
        BM->toFD = 0;
    kill(ppid, SIGUSR1);

    while (BM->used)
        continue;
}

void ServerBroadcast()
{
    for (int i = 0; i < MAX_BROADCAST; i++)
    {
        if (BM[i].used)
        {
            string output(BM[i].msg);
            if (BM[i].toFD != 0)
            {
                if (send(BM[i].toFD, output.c_str(), output.size(), 0) < 0)
                    perror("broadcast/send");
            }
            else
            {
                for (int index = 1; index < MAX_CLIENT; index++)
                    if (clients[index].used)
                        if (send(clients[index].fd, output.c_str(), output.size(), 0) < 0)
                            perror("broadcast/send");
            }
            BM[i].used = false;
        }
    }
}