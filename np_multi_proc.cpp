#include "np_multi_proc.h"

const char welcome[] =
    "****************************************\n\
** Welcome to the information server. **\n\
****************************************\n";

int serverOutfd = 300;
/*
 * return 1 with no error and continue
 * return 0 with no error but stop
 * return -1 with error
 */
int shellwithFD(int fd)
{
    signal(SIGCHLD, signalHandler);
    signal(SIGINT, signalHandler);
    signal(SIGUSR2, signalHandler);
    clearenv();
    setenv("PATH", "bin:.", 1);

    for (int i = 1; i < MAX_CLIENT; i++)
    {
        if (clients[i].fd == fd)
        {
            me = &clients[i];
            myIndex = i;
            break;
        }
    }

    // tmp variable
    string s;
    cout << "% ";
    while (getline(cin, s))
    {
        if (s == "")
        {
            cout << "% ";
            continue;
        }

        // cout<<s.size()<<endl;
        s.erase(remove(s.begin(), s.end(), '\n'), s.end());
        s.erase(remove(s.begin(), s.end(), '\r'), s.end());
        // cout<<s.size()<<endl;
        broadcast(SERVER,0,s,0,0);

        // 分割當前的指令
        vector<string> lineSplit;
        string delimiter = " ";

        size_t pos = 0;
        string token;
        int rc;
        while ((pos = s.find(delimiter)) != string::npos)
        {
            token = s.substr(0, pos);
            lineSplit.push_back(token);
            s.erase(0, pos + delimiter.length());
            if (token[0] == '|' && token.size() > 1)
            {
                rc = parserCommand(lineSplit);
                if (rc < 1)
                    return (rc);
                lineSplit.clear();
            }
        }

        lineSplit.push_back(s);

        rc = parserCommand(lineSplit);
        if (rc < 1)
            return (rc);

        cout << "% ";
    }
    return 1;
}

int findTheGlobalPipeCanUse()
{
    for (int i = 0; i < 1000; i++)
    {
        if (!GlobalPipeUsed[i])
        {
            GlobalPipeUsed[i] = !GlobalPipeUsed[i];

            if (pipe(GlobalPipe[i]) == -1)
            {
                cerr << "GlobalPipe gen failed" << endl;
                exit(-1);
            }

            return i;
        }
    }
    cerr << "All GlobalPipe Used" << endl;
    return -1;
}

/*
 * return 1 with no error and continue
 * return 0 with no error but stop
 * return -1 with error
 */
int parserCommand(vector<string> SeperateInput)
{
    int NumberPipeNeed = -1, indexInNumberPipe = -1;
    for (int j = 0; j < NumberPipeArray.size(); j++)
    {
        NumberPipeArray[j].number = NumberPipeArray[j].number - 1;
        if (NumberPipeArray[j].number == 0)
        {
            indexInNumberPipe = j;
            NumberPipeNeed = NumberPipeArray[j].IndexOfGlobalPipe;
        }
    }

    if (SeperateInput[0] == "printenv")
    {
        printf("%s\n", printEnv(SeperateInput[1]));

        return 1;
    }
    else if (SeperateInput[0] == "setenv")
    {

        return setEnv(SeperateInput[1], SeperateInput[2]);
    }
    else if (SeperateInput[0] == "exit")
    {
        exit(0);
    }
    else if (SeperateInput[0] == "name")
    {
        if (SeperateInput.size() > 2)
        {
            cout << "only one word behind name: [name id]" << endl;

            return 1;
        }
        name(SeperateInput[1]);
        return 1;
    }
    else if (SeperateInput[0] == "who")
    {
        who();
        return 1;
    }
    else if (SeperateInput[0] == "yell")
    {
        string msg = "";
        for (int i = 1; i < SeperateInput.size(); i++)
            if (i != 1)
                msg += (" " + SeperateInput[i]);
            else
                msg += SeperateInput[i];

        broadcast(YELL, me->fd, msg, 0, 0);

        return 1;
    }
    else if (SeperateInput[0] == "tell")
    {
        int index = stoi(SeperateInput[1]);
        if ((index < 1 || index > MAX_CLIENT) || !clients[index].used)
        {
            broadcast(ERROR_USER, me->fd, "", 0, index);
            return 1;
        }
        string msg = "";
        for (int i = 2; i < SeperateInput.size(); i++)
            if (i != 2)
                msg += (" " + SeperateInput[i]);
            else
                msg += SeperateInput[i];

        broadcast(TELL, me->fd, msg, clients[index].fd, 0);
        return 1;
    }

    pid_t pid, wpid;
    int status = 0;

    string msg;
    int count = 0, parseCommandLine = 0, pipeNumber = 0;
    // userpipe variable
    int from, to;
    vector<myCommandLine> parseCommand;
    parseCommand.resize(1);

    int myIndex;
    for (int clientIndex = 1; clientIndex < MAX_CLIENT; ++clientIndex)
    {
        if (clients[clientIndex].used && clients[clientIndex].fd == me->fd)
        {
            myIndex = clientIndex;
        }
    }

    while (count < SeperateInput.size())
    {

        if (SeperateInput[count][0] == '|' || SeperateInput[count][0] == '!')
        {
            if (SeperateInput[count][0] == '!')
            {
                parseCommand[parseCommandLine].errPipeNeed = true;
            }

            // * 實作numberPipe
            if (SeperateInput[count].size() > 1)
            {
                SeperateInput[count].erase(SeperateInput[count].begin());
                int Number = atoi(SeperateInput[count].c_str());

                bool hasPipe = false;
                parseCommand[parseCommandLine].numberPipe = true;

                for (int k = 0; k < NumberPipeArray.size(); k++)
                {
                    if (NumberPipeArray[k].number == Number)
                    {
                        hasPipe = true;
                        parseCommand[parseCommandLine].numberPipeIndex = NumberPipeArray[k].IndexOfGlobalPipe;
                    }
                }

                if (!hasPipe)
                {
                    int GlobalPipeIndex = findTheGlobalPipeCanUse();
                    myNumberPipe nP;
                    nP.number = Number;
                    nP.IndexOfGlobalPipe = GlobalPipeIndex;
                    NumberPipeArray.push_back(nP);
                    parseCommand[parseCommandLine].numberPipeIndex = GlobalPipeIndex;
                }
            }

            //* 實作普通的pipe
            else if (count != SeperateInput.size() - 1)
            {
                myCommandLine newCommand;
                parseCommand.push_back(newCommand);
                parseCommandLine++;
            }
        }

        // user pipe
        else if ((SeperateInput[count][0] == '<' || SeperateInput[count][0] == '>') && SeperateInput[count].size() > 1)
        {
            msg = SeperateInput[0];
            for (int i = 1; i < SeperateInput.size(); i++)
                msg += (" " + SeperateInput[i]);

            string copyString = SeperateInput[count];
            copyString.erase(copyString.begin());
            int targetIndex = atoi(copyString.c_str());

            if (targetIndex == myIndex)
            {
                cout << "can not pass to yourself" << endl;
                return 1;
            }

            // targetIndex 是 對方的clients index not FD Number;
            // fifo file 是 自己的index*30 + 對方的index 所以自己的index*30+ 0~30 都是自己的fifo
            if (SeperateInput[count][0] == '<')
            { // 接收
                to = myIndex;
                from = targetIndex;

                if (clients[from].used && userPipeFDArray[from] == 0)
                {
                    //* 不存在pipe
                    broadcast(ERROR_PIPE_NOT_EXIST, me->fd, "", 0, from);
                    parseCommand[parseCommandLine].pipeFrom = 0;
                }
                else
                    //* 存在pipe 可以儲存pipeInfrom
                    parseCommand[parseCommandLine].pipeFrom = from;
                parseCommand[parseCommandLine].fifoFromFD = userPipeFDArray[from];
            }
            else
            { // 傳出
                to = targetIndex;
                from = myIndex;
                string fifoFileName = "./user_pipe/" + to_string(to * 30 + from);

                // TODO: 1.創建對方的map 2. 創建對方對應自己的map 3. 建立pipe
                //* 1. 確定對方是否存在與對方的map是否存在 存在進2. 不存在將其建立起來
                if (!(to > 0 && to < MAX_CLIENT && clients[to].used))
                {
                    parseCommand[parseCommandLine].pipeTo = to;
                    count++;
                    continue;
                }

                //* 2. 確認對方對於自己的pipe是否存在 存在表示前面pipe過東西對方還沒接收 不存在即可存進3.
                if (mknod(fifoFileName.c_str(), S_IFIFO | 0777, 0) < 0)
                {
                    if (errno != EEXIST)
                    {
                        perror("mknod");
                    }
                    else
                    {
                        broadcast(ERROR_PIPE_IS_EXIST, me->fd, "", 0, to);
                        parseCommand[parseCommandLine].pipeTo = 0;
                        count++;
                        continue;
                    }
                }
                // cout << "kill sigusr2 pid:" << clients[targetIndex].pid << endl;
                kill(clients[targetIndex].pid, SIGUSR2);

                //* 3. 建立pipe並儲存pipeInform到parseCommand[parseCommandLine]
                int writeFD = open(fifoFileName.c_str(), 1);
                if (writeFD < 0)
                    perror("open");
                parseCommand[parseCommandLine].fifoToFD = writeFD;
                parseCommand[parseCommandLine].pipeTo = to;
            }
        }

        else
            parseCommand[parseCommandLine].inputCommand.push_back(SeperateInput[count]);

        count++;
    }

    int pipeArray[2][2];
    for (int i = 0; i < parseCommand.size(); i++)
    {
        if (pipe(pipeArray[i % 2]) < 0)
            perror("pipe gen failed");

        // * handle userPipe in
        if (parseCommand[i].pipeFrom != -1)
        {
            if (parseCommand[i].pipeFrom >= MAX_CLIENT || parseCommand[i].pipeFrom <= 0 || !clients[parseCommand[i].pipeFrom].used || parseCommand[i].pipeFrom == 0)
            {
                if (parseCommand[i].pipeFrom != 0)
                    broadcast(ERROR_USER, me->fd, "", 0, parseCommand[i].pipeFrom);
                parseCommand[i].pipeFrom = 0;
            }
            else
                broadcast(RECV, me->fd, msg, clients[parseCommand[i].pipeFrom].fd, 0);
        }

        // * handle userPipe Out
        if (parseCommand[i].pipeTo != -1)
        {
            if (parseCommand[i].pipeTo >= MAX_CLIENT || parseCommand[i].pipeTo <= 0 || !clients[parseCommand[i].pipeTo].used || parseCommand[i].pipeTo == 0)
            {
                if (parseCommand[i].pipeTo != 0)
                    broadcast(ERROR_USER, me->fd, "", 0, parseCommand[i].pipeTo);
                parseCommand[i].pipeTo = 0;
            }
            else
                broadcast(SEND, me->fd, msg, clients[parseCommand[i].pipeTo].fd, 0);
        }

        pid = fork();
        if (pid == 0) // child process
        {
            // * front Pipe
            if (i > 0)
            {
                close(pipeArray[(i - 1) % 2][1]);
                dup2(pipeArray[(i - 1) % 2][0], 0);
                close(pipeArray[(i - 1) % 2][0]);
            }

            // * back Pipe
            if (i != parseCommand.size() - 1)
            {
                close(pipeArray[i % 2][0]);
                dup2(pipeArray[i % 2][1], 1);
                if (parseCommand[i].errPipeNeed)
                {
                    dup2(GlobalPipe[i % 2][1], 2);
                }
                close(pipeArray[i % 2][1]);
            }

            // * number Pipe behind
            if (parseCommand[i].numberPipe)
            {
                // cerr<<"numberpipe"<<endl;
                int NumberPipeIndex = parseCommand[i].numberPipeIndex;
                close(GlobalPipe[NumberPipeIndex][0]);
                dup2(GlobalPipe[NumberPipeIndex][1], 1);
                if (parseCommand[i].errPipeNeed)
                {
                    dup2(GlobalPipe[NumberPipeIndex][1], 2);
                }
                close(GlobalPipe[NumberPipeIndex][1]);
            }

            //  * handle numberPipe stdIn
            if (i == 0 && NumberPipeNeed != -1)
            {
                close(GlobalPipe[NumberPipeNeed][1]);
                dup2(GlobalPipe[NumberPipeNeed][0], 0);
                close(GlobalPipe[NumberPipeNeed][0]);
            }

            // * handle userPipe in
            if (parseCommand[i].pipeFrom != -1)
            {
                if (parseCommand[i].pipeFrom == 0)
                {
                    int nul = open("/dev/null", O_RDWR);
                    dup2(nul, 0);
                    close(nul);
                }
                else
                {
                    dup2(parseCommand[i].fifoFromFD, 0);
                    close(parseCommand[i].fifoFromFD);
                }
            }

            // * handle userPipe Out
            if (parseCommand[i].pipeTo != -1)
            {
                if (parseCommand[i].pipeTo == 0)
                {
                    int nul = open("/dev/null", O_RDWR);
                    dup2(nul, 1);
                    close(nul);
                }
                else
                {
                    dup2(parseCommand[i].fifoToFD, 1);
                    close(parseCommand[i].fifoToFD);
                }
            }

            executeFunction(parseCommand[i]);
        }
        else if (pid > 0) // parent  process
        {
            // * close pipe
            if (i > 0)
            {
                close(pipeArray[(i - 1) % 2][0]);
                close(pipeArray[(i - 1) % 2][1]);
            }

            // * close numberPipe
            if (i == 0 && NumberPipeNeed != -1)
            {
                close(GlobalPipe[NumberPipeNeed][0]);
                close(GlobalPipe[NumberPipeNeed][1]);
                GlobalPipeUsed[NumberPipeNeed] = false;
                NumberPipeArray.erase(NumberPipeArray.begin() + indexInNumberPipe);
            }

            if (parseCommand.size() - 1 == i)
                if (!parseCommand[i].numberPipe && !(parseCommand[i].pipeTo != -1))
                {
                    while ((wpid = wait(&status)) > 0)
                    {
                    };
                }

            // * close & unlink userpipe 在wait之後
            if (parseCommand[i].pipeFrom != -1 && !(parseCommand[i].pipeFrom >= MAX_CLIENT || parseCommand[i].pipeFrom <= 0 || !clients[parseCommand[i].pipeFrom].used))
            {
                close(parseCommand[i].fifoFromFD);
                userPipeFDArray[parseCommand[i].pipeFrom] = 0;
                string fifoFileName = USERPIPE_PATH + to_string(myIndex * 30 + parseCommand[i].pipeFrom);
                if (unlink(fifoFileName.c_str()) < 0)
                    perror("unlink");
            }

            if (parseCommand[i].pipeTo != -1)
                close(parseCommand[i].fifoToFD);
        }
        else // fork error
        {
            // cerr << "fork error" << endl;
            close(pipeArray[i % 2][0]);
            close(pipeArray[i % 2][1]);
            i--;
            continue;
        }
    }
    return 1;
}

void executeFunction(myCommandLine tag)
{
    const char **arg = new const char *[tag.inputCommand.size() + 1];
    for (int i = 0; i < tag.inputCommand.size(); i++)
    {
        if (tag.inputCommand[i] == ">")
        {
            int fd = open(tag.inputCommand[i + 1].c_str(), O_CREAT | O_RDWR | O_TRUNC, S_IREAD | S_IWRITE);
            if (fd < 0)
            {
                cerr << "open failed" << endl;
            }

            if (dup2(fd, 1) < 0)
            {
                cerr << "dup error" << endl;
            }
            close(fd);
            arg[i] = NULL;
            break;
        }

        arg[i] = tag.inputCommand[i].c_str();
    }
    arg[tag.inputCommand.size()] = NULL;

    if (execvp(tag.inputCommand[0].c_str(), (char **)arg) == -1)
    {
        cerr << "Unknown command: [" << tag.inputCommand[0] << "]." << endl;
        exit(-1);
    };
}

void who()
{
    // for (int index = 1; index < 5; index++)
    // {
    //     printClients(index);
    // }
    printf("<ID>\t<nickname>\t<IP:port>\t<indicate me>\n");
    for (int index = 1; index < MAX_CLIENT; index++)
    {
        client *c = &clients[index];
        if (c->used)
            if (c->fd == me->fd)
            {
                printf("%d\t%s\t%s\t<-me\n", index, c->name, c->ip);
            }
            else
            {
                printf("%d\t%s\t%s\t\n", index, c->name, c->ip);
            }
    }
}

void name(string name)
{
    bool havSame = false;
    int clientIndex = -1;
    for (int index = 1; index < MAX_CLIENT; index++)
    {
        if (!clients[index].used)
            continue;
        if (clients[index].name == name)
        {
            havSame = true;
            break;
        }
        if (clients[index].fd == me->fd)
        {
            clientIndex = index;
        }
    }
    if (havSame)
    {
        printf("*** User '%s' already exists. ***\n", name.c_str());
        fflush(stdout);
        return;
    }
    strcpy(clients[clientIndex].name, name.c_str());
    broadcast(NAME, clients[clientIndex].fd, "", 0, 0);
    return;
}

char *printEnv(string variable)
{
    if (getenv(variable.c_str()) != NULL)
        return getenv(variable.c_str());
}
int setEnv(string s1, string s2)
{
    if (setenv(s1.c_str(), s2.c_str(), 1) == -1)
    {
        cerr << "setenv error" << endl;
        exit(-1);
    };

    return 1;
}

void logoutControl(int pid)
{
    cout << "[Server] clients ptr:" << clients << endl;
    int fd, closeClientIndex;
    for (int index = 1; index < MAX_CLIENT; index++)
    {
        if (clients[index].used && clients[index].pid == pid)
        {
            printClients(index);
            closeClientIndex = index;
            broadcast(LOGOUT, clients[index].fd, "", 0, 0);
            clients[index].used = false;
            close(clients[index].fd);
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

void signalHandler(int sig)
{
    if (sig == SIGCHLD)
        pid_t pid = wait(NULL);
    else if (sig == SIGINT)
    {
        // cout << "\nchild SIGINT accept" << endl;
        exit(0);
    }
    else if (sig == SIGUSR2)
    {
        // cout << "accept signal sigusr2" << endl;
        // TODO: create , open FIFO read
        for (int i = 1; i < MAX_CLIENT; i++)
        {
            if (clients[i].used && userPipeFDArray[i] == 0)
            {
                string fifoName = USERPIPE_PATH + to_string(myIndex * 30 + i);
                int readFD = open(fifoName.c_str(), 0);
                if (readFD < 0) // FIFO 不存在
                    continue;
                userPipeFDArray[i] = readFD;
            }
        }
    }
    else
        cout << "accept sig:" << sig << endl;
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
        int pid, status;
        pid = wait(&status);
        cout << "[SigChld]: child pid " << pid << " exit" << endl;
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

        // for (int i = 1; i < MAX_CLIENT; i++)
        // {
        //     for (int j = 1; j < MAX_CLIENT; j++)
        //     {
        //         string fifoFileName = "./user_pipe/" + to_string(30 * (i) + j);
        //         if ((unlink(fifoFileName.c_str()) < 0) && (errno != ENOENT))
        //             perror("unlink");
        //     }
        // }

        if (rmdir(USERPIPE_PATH.c_str()) < 0)
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

void printClients(int h)
{
    printf("clients[%d]: \n", h);
    printf("\t used:\t%d\n", clients[h].used);
    printf("\t fd:\t%d\n", clients[h].fd);
    printf("\t pid:\t%d\n", clients[h].pid);
    printf("\t name:\t%s\n", clients[h].name);
}

void initMMap()
{
    csfile = SM_PATH + "clientsSharedMemory";
    clients_shared_momory_fd = open(csfile.c_str(), O_CREAT | O_RDWR, 00777);
    ftruncate(clients_shared_momory_fd, sizeof(client) * MAX_CLIENT);
    clients = (client *)mmap(NULL, sizeof(client) * MAX_CLIENT, PROT_READ | PROT_WRITE, MAP_SHARED, clients_shared_momory_fd, 0);
    if (clients == MAP_FAILED)
        perror("mmap");
    cout<<"[Server] mmap init with clients ptr: "<<clients<<endl;

    bmfile = SM_PATH + "broadcastSharedMemory";
    broadcast_shared_momory_fd = open(bmfile.c_str(), O_CREAT | O_RDWR, 00777);
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
    dup2(1,serverOutfd);

    if (NULL == opendir(USERPIPE_PATH.c_str()))
        mkdir(USERPIPE_PATH.c_str(), 0777);
    if (NULL == opendir(SM_PATH.c_str()))
        mkdir(SM_PATH.c_str(), 0777);

    initMMap();
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
        cout << "[Server]: waiting a accept" << endl;
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
            // close(ssock);
            cout << "end" << endl;
        }
        else
        {
            send(ssock, welcome, sizeof(welcome) - 1, 0);
            broadcast(LOGIN, ssock, "", 0, 0);
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
        sprintf(buf, "*** Error: the pipe #%d->#%d does not exist yet. ***\n", targetIndex, indexFrom);
        break;
    case ERROR_PIPE_IS_EXIST:
        sprintf(buf, "*** Error: the pipe #%d->#%d already exists. ***\n", indexFrom, targetIndex);
        break;
    case SERVER:
        sprintf(buf,"%s",msg.c_str());
        break;
    default:
        break;
    }

    if (type == TELL)
    {
        BM->toFD = targetFD;
    }
    else if(type==SERVER){
        BM->toFD = msock;
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
            else if(BM[i].toFD==msock){
                cout<<output<<endl;
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