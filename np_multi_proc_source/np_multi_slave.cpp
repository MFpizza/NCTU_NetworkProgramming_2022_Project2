#include "np_multi_proc.h"
#include "np_multi_slave.h"
#include <algorithm>
using namespace std;

// slave global variable
int myIndex;
client *me;
int userPipeFDArray[31] = {0};
vector<myNumberPipe> number_pipe_array;

void executeFunction(myCommandLine tag);
int parserCommand(vector<string> SeperateInput);
void broadcast(BROADCAST_TYPE type, int fromFD, string msg, int targetFD, int targetIndex);
void who();
void name(string name);
char *printEnv(string variable);
int setEnv(string s1, string s2);
void signalHandler(int sig);

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

int shellwithFD(int fd)
{
    signal(SIGCHLD, signalHandler);
    signal(SIGINT, signalHandler);
    signal(SIGUSR2, signalHandler);
    clearenv();
    setenv("PATH", "bin:.", 1);

    myIndex = 1;
    while (true)
    {
        if (clients[myIndex].pid == getpid())
        {
            me = &clients[myIndex];
            break;
        }
        myIndex++;
        if (myIndex > MAX_CLIENT)
            myIndex = 1;
    }
    broadcast(LOGIN, me->fd, "", 0, 0);

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

        // echo 指令
        broadcast(SERVER, 0, s, 0, 0);

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

void broadcast(BROADCAST_TYPE type, int fromFD, string msg, int targetFD, int targetIndex)
{
    int indexTarget;
    for (int i = 1; i < MAX_CLIENT; i++)
    {
        if (!clients[i].used)
            continue;
        else if (clients[i].fd == targetFD)
            indexTarget = i;
    }

    broadcastMsg *bmPtr = &BM[myIndex];

    char *buf = bmPtr->msg;
    memset(buf, 0, sizeof(char) * BUFSIZE);
    switch (type)
    {
    case LOGIN:
        sprintf(buf, "*** User '%s' entered from %s. ***\n", me->name, me->ip);
        break;
    case LOGOUT:
        sprintf(buf, "*** User '%s' left. ***\n", me->name);
        break;
    case NAME:
        sprintf(buf, "*** User from %s is named '%s'. ***\n", me->ip, me->name);
        break;
    case YELL:
        sprintf(buf, "*** %s yelled ***: %s\n", me->name, msg.c_str());
        break;
    case TELL:
        sprintf(buf, "*** %s told you ***: %s\n", me->name, msg.c_str());
        break;
    case SEND:
        sprintf(buf, "*** %s (#%d) just piped '%s' to %s (#%d) ***\n", me->name, myIndex, msg.c_str(), clients[indexTarget].name, indexTarget);
        break;
    case RECV:
        sprintf(buf, "*** %s (#%d) just received from %s (#%d) by '%s' ***\n", me->name, myIndex, clients[indexTarget].name, indexTarget, msg.c_str());
        break;
    case ERROR_USER:
        sprintf(buf, "*** Error: user #%d does not exist yet. ***\n", targetIndex);
        break;
    case ERROR_PIPE_NOT_EXIST:
        sprintf(buf, "*** Error: the pipe #%d->#%d does not exist yet. ***\n", targetIndex, myIndex);
        break;
    case ERROR_PIPE_IS_EXIST:
        sprintf(buf, "*** Error: the pipe #%d->#%d already exists. ***\n", myIndex, targetIndex);
        break;
    case SERVER:
        sprintf(buf, "%d: %s", myIndex, msg.c_str());
        break;
    default:
        break;
    }

    if (type == TELL)
        bmPtr->toFD = targetFD;
    else if (type == SERVER)
        bmPtr->toFD = clients[0].fd;
    else if (type == ERROR_USER || type == ERROR_PIPE_NOT_EXIST || type == ERROR_PIPE_IS_EXIST)
        bmPtr->toFD = me->fd;
    else
        bmPtr->toFD = 0;
    bmPtr->used = true;
    kill(clients[0].pid, SIGUSR1);
    while (bmPtr->used)
        continue;
}

int parserCommand(vector<string> SeperateInput)
{
    int indexInNumberPipe = -1;
    for (int j = 0; j < number_pipe_array.size(); j++)
    {
        number_pipe_array[j].number = number_pipe_array[j].number - 1;
        if (number_pipe_array[j].number == 0)
            indexInNumberPipe = j;
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
    int parseCommandLine = 0;
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

    for (int count = 0; count < SeperateInput.size(); count++)
    {
        // * implement pipe, number pipe and error pipe
        if (SeperateInput[count][0] == '|' || SeperateInput[count][0] == '!')
        {
            // * error pipe
            if (SeperateInput[count][0] == '!')
            {
                parseCommand[parseCommandLine].errPipeNeed = true;
            }

            // * number_pipe part
            if (SeperateInput[count].size() > 1) // && count == SeperateInput.size()-1
            {
                SeperateInput[count].erase(SeperateInput[count].begin());
                int Number = atoi(SeperateInput[count].c_str());

                bool hasPipe = false;
                parseCommand[parseCommandLine].numberPipe = true;
                for (int k = 0; k < number_pipe_array.size(); k++)
                {
                    if (number_pipe_array[k].number == Number)
                    {
                        hasPipe = true;
                        parseCommand[parseCommandLine].numberPipeIndex = k;
                    }
                }

                if (!hasPipe)
                {
                    myNumberPipe nP;
                    if (pipe(nP.numberPipe) < 0)
                        perror("numberPipe");
                    nP.number = Number;
                    number_pipe_array.push_back(nP);
                    parseCommand[parseCommandLine].numberPipeIndex = number_pipe_array.size() - 1;
                }
            }

            // * normal pipe
            else if (count != SeperateInput.size() - 1)
            {
                myCommandLine newCommand;
                parseCommand.push_back(newCommand);
                parseCommandLine++;
            }
        }

        // * user pipe
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
    }

    int pipeArray[2][2];
    for (int i = 0; i < parseCommand.size(); i++)
    {
        if (parseCommand.size() > 1)
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
                    dup2(pipeArray[i % 2][1], 2);
                close(pipeArray[i % 2][1]);
            }

            // * number Pipe behind
            if (parseCommand[i].numberPipe)
            {
                // cerr<<"numberpipe"<<endl;
                int *pipeTo = number_pipe_array[parseCommand[i].numberPipeIndex].numberPipe;
                close(pipeTo[0]);
                dup2(pipeTo[1], 1);
                if (parseCommand[i].errPipeNeed)
                    dup2(pipeTo[1], 2);
                close(pipeTo[1]);
            }

            //  * handle numberPipe stdIn
            if (i == 0 && indexInNumberPipe != -1)
            {
                int *ptr = number_pipe_array[indexInNumberPipe].numberPipe;
                close(ptr[1]);
                dup2(ptr[0], 0);
                close(ptr[0]);
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
            if (i == 0 && indexInNumberPipe != -1)
            {
                close(number_pipe_array[indexInNumberPipe].numberPipe[0]);
                close(number_pipe_array[indexInNumberPipe].numberPipe[1]);
                number_pipe_array.erase(number_pipe_array.begin() + indexInNumberPipe);
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
                cerr << "open failed" << endl;
            if (dup2(fd, 1) < 0)
                cerr << "dup error" << endl;
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
