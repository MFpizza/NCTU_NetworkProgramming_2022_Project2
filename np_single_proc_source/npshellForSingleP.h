#include <iostream>
#include <string.h>
#include <vector>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <algorithm>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <arpa/inet.h>
#include <map>
using namespace std;
#define LISTEN_BACKLOG 50
#define QLEN 5
#define BUFSIZE 4096
typedef enum
{
    LOGIN = 0,
    LOGOUT,
    NAME,
    YELL,
    TELL,
    SEND,
    RECV,
    ERROR_USER,
    ERROR_PIPE_NOT_EXIST,
    ERROR_PIPE_IS_EXIST
} BROADCAST_TYPE;

struct myNumberPipe
{
    int number; // Next number time to pipe the output
    int numberPipe[2];
};

struct myCommandLine
{
    vector<string> inputCommand; // store the command line
    bool numberPipe = false;     // true if there is a number pipe command
    int numberPipeIndex = -1;    // 還在思考要用甚麼方式來儲存numberPipe
    bool errPipeNeed = false;    // true if there is a pipe command be
    int pipeTo = -1;             // index of userpipe To
    int pipeFrom = -1;           // index of userpipe From
};

struct client
{
    client(int _fd, string _name, string _ip)
    {
        fd = _fd;
        name = _name;
        ip = _ip;
    }
    int fd;
    string name;
    string ip;
    vector<myNumberPipe> number_pipe_array;
    int GlobalPipe[1000][2];
    bool GlobalPipeUsed[1000];
    map<string, string> environment;
};

int msock; // master socket fd
int nfds;
fd_set rfds;
fd_set afds;
vector<client *> clients; // [0] == null others is client

map<int, map<int, int *>> userPipe;
// 格式 本身自己的fd : 對方的fd 要傳入自己本身的對應的pipe

void executeFunction(myCommandLine tag);
int parserCommand(vector<string> SeperateInput, int fd, client *c);
void broadcast(BROADCAST_TYPE type, client *from, string msg, int targetFD, int targetIndex);
void who(int fd);
void name(int fd, string name);
void logoutControl(int fd);
char *printEnv(string variable, client *c);
int setEnv(string s1, string s2, client *c);

void signalHandler(int sig)
{
    pid_t pid = wait(NULL);
}

const char welcome[] =
    "****************************************\n\
** Welcome to the information server. **\n\
****************************************\n";

/*
 * return 1 with no error and continue
 * return 0 with no error but stop
 * return -1 with error
 */
int shellwithFD(int fd)
{
    char buf[BUFSIZE];
    memset(buf, 0, sizeof(char) * BUFSIZE);
    int cc;
    cc = recv(fd, buf, BUFSIZE, 0);
    if (cc == 0)
        return 0;

    else if (cc < 0)
        return -1;

    buf[cc] = '\0';

    client *c;
    for (int i = 1; i < clients.size(); i++)
    {
        if (clients[i] != NULL && clients[i]->fd == fd)
            c = clients[i];
    }

    signal(SIGCHLD, signalHandler);
    clearenv();
    setenv("PATH", printEnv("PATH", c), 1);

    // tmp variable
    string s(buf);

    if (s == "")
    {
        return 0;
    }

    // cout<<s.size()<<endl;
    s.erase(remove(s.begin(), s.end(), '\n'), s.end());
    s.erase(remove(s.begin(), s.end(), '\r'), s.end());
    // cout<<s.size()<<endl;
    // cout<<s<<endl;

    dup2(fd, 1);
    dup2(fd, 2);
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
            rc = parserCommand(lineSplit, fd, c);
            if (rc < 1)
                return (rc);
            lineSplit.clear();
        }
    }

    lineSplit.push_back(s);

    rc = parserCommand(lineSplit, fd, c);
    if (rc < 1)
        return (rc);

    send(fd, "% ", 2, 0);
    return 1;
}

int parserCommand(vector<string> SeperateInput, int fd, client *c)
{
    int indexInNumberPipe = -1;
    for (int j = 0; j < c->number_pipe_array.size(); j++)
    {
        c->number_pipe_array[j].number = c->number_pipe_array[j].number - 1;
        if (c->number_pipe_array[j].number == 0)
            indexInNumberPipe = j;
    }

    if (SeperateInput[0] == "printenv")
    {
        printf("%s\n", printEnv(SeperateInput[1], c));
        return 1;
    }
    else if (SeperateInput[0] == "setenv")
    {
        return setEnv(SeperateInput[1], SeperateInput[2], c);
    }
    else if (SeperateInput[0] == "exit")
    {
        return 0;
    }
    else if (SeperateInput[0] == "name")
    {
        if (SeperateInput.size() > 2)
        {
            cout << "only one word behind name: [name id]" << endl;
            return 1;
        }
        name(fd, SeperateInput[1]);
        return 1;
    }
    else if (SeperateInput[0] == "who")
    {
        who(fd);
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

        broadcast(YELL, c, msg, 0, 0);
        return 1;
    }
    else if (SeperateInput[0] == "tell")
    {
        int index = stoi(SeperateInput[1]);
        if ((index < 1 || index > clients.size()) || clients[index] == NULL)
        {
            broadcast(ERROR_USER, c, "", 0, index);
            return 1;
        }
        string msg = "";
        for (int i = 2; i < SeperateInput.size(); i++)
            if (i != 2)
                msg += (" " + SeperateInput[i]);
            else
                msg += SeperateInput[i];

        broadcast(TELL, c, msg, clients[index]->fd, 0);
        return 1;
    }

    pid_t pid, wpid;
    int status = 0;

    string msg;
    int parseCommandLine = 0, pipeNumber = 0;
    // userpipe variable
    int from, to;
    vector<myCommandLine> parseCommand;
    parseCommand.resize(1);

    int myIndex;
    for (int clientIndex = 1; clientIndex < clients.size(); ++clientIndex)
    {
        if (clients[clientIndex] != NULL && clients[clientIndex]->fd == fd)
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
                for (int k = 0; k < c->number_pipe_array.size(); k++)
                {
                    if (c->number_pipe_array[k].number == Number)
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
                    c->number_pipe_array.push_back(nP);
                    parseCommand[parseCommandLine].numberPipeIndex = c->number_pipe_array.size() - 1;
                }
            }

            //* normal pipe
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
            if (SeperateInput[count][0] == '<')
            { // 接收
                to = myIndex;
                from = targetIndex;
                if ((from < clients.size() && from > 0 && clients[from] != NULL) && (!userPipe[to].count(from)))
                {
                    //* 不存在pipe
                    broadcast(ERROR_PIPE_NOT_EXIST, c, "", 0, from);
                    parseCommand[parseCommandLine].pipeFrom = 0;
                }
                else
                    //* 存在pipe 可以儲存pipeInfrom
                    parseCommand[parseCommandLine].pipeFrom = from;
            }
            else
            { // 傳出
                to = targetIndex;
                from = myIndex;

                // TODO: 1.創建對方的map 2. 創建對方對應自己的map 3. 建立pipe
                //* 1. 確定對方是否存在與對方的map是否存在 存在進2. 不存在將其建立起來
                if (!(to > 0 && to < clients.size() && clients[to] != NULL))
                {
                    parseCommand[parseCommandLine].pipeTo = to;
                    count++;
                    continue;
                }

                if (!userPipe.count(to))
                {
                    map<int, int *> newChildMap;
                    userPipe[to] = newChildMap;
                }

                //* 2. 確認對方對於自己的pipe是否存在 存在表示前面pipe過東西對方還沒接收 不存在即可存進3.
                if (userPipe[to].count(from))
                {
                    broadcast(ERROR_PIPE_IS_EXIST, c, "", 0, to);
                    parseCommand[parseCommandLine].pipeTo = 0;
                    count++;
                    continue;
                }

                //* 3. 建立pipe並儲存pipeInform到parseCommand[parseCommandLine]
                userPipe[to][from] = new int[2]();
                if (pipe(userPipe[to][from]) < 0)
                    perror("userPipe error");

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
            if (parseCommand[i].pipeFrom >= clients.size() || parseCommand[i].pipeFrom <= 0 || clients[parseCommand[i].pipeFrom] == NULL || parseCommand[i].pipeFrom == 0)
            {
                if (parseCommand[i].pipeFrom != 0)
                    broadcast(ERROR_USER, c, "", 0, parseCommand[i].pipeFrom);
                parseCommand[i].pipeFrom = 0;
            }
            else
                broadcast(RECV, c, msg, clients[parseCommand[i].pipeFrom]->fd, 0);
        }

        // * handle userPipe Out
        if (parseCommand[i].pipeTo != -1)
        {
            if (parseCommand[i].pipeTo >= clients.size() || parseCommand[i].pipeTo <= 0 || clients[parseCommand[i].pipeTo] == NULL || parseCommand[i].pipeTo == 0)
            {
                if (parseCommand[i].pipeTo != 0)
                    broadcast(ERROR_USER, c, "", 0, parseCommand[i].pipeTo);
                parseCommand[i].pipeTo = 0;
            }
            else
                broadcast(SEND, c, msg, clients[parseCommand[i].pipeTo]->fd, 0);
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
                int *pipeTo = c->number_pipe_array[parseCommand[i].numberPipeIndex].numberPipe;
                close(pipeTo[0]);
                dup2(pipeTo[1], 1);
                if (parseCommand[i].errPipeNeed)
                    dup2(pipeTo[1], 2);
                close(pipeTo[1]);
            }

            //  * handle numberPipe stdIn
            if (i == 0 && indexInNumberPipe != -1)
            {
                int *ptr = c->number_pipe_array[indexInNumberPipe].numberPipe;
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
                    int *ptr = userPipe[myIndex][parseCommand[i].pipeFrom];
                    close(ptr[1]);
                    dup2(ptr[0], 0);
                    close(ptr[0]);
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
                    int *ptr = userPipe[parseCommand[i].pipeTo][myIndex];
                    close(ptr[0]);
                    dup2(ptr[1], 1);
                    close(ptr[1]);
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
                close(c->number_pipe_array[indexInNumberPipe].numberPipe[0]);
                close(c->number_pipe_array[indexInNumberPipe].numberPipe[1]);
                c->number_pipe_array.erase(c->number_pipe_array.begin() + indexInNumberPipe);
            }

            // * close userpipe
            if (parseCommand[i].pipeFrom != -1 && !(parseCommand[i].pipeFrom >= clients.size() || parseCommand[i].pipeFrom <= 0 || clients[parseCommand[i].pipeFrom] == NULL))
            {
                int *ptr = userPipe[myIndex][parseCommand[i].pipeFrom];
                close(ptr[1]);
                close(ptr[0]);
                userPipe[myIndex].erase(parseCommand[i].pipeFrom);
            }
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
    if (!parseCommand[parseCommandLine].numberPipe && !(parseCommand[parseCommandLine].pipeTo != -1))
        while ((wpid = wait(NULL)) > 0)
            continue;
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

void who(int fd)
{
    printf("<ID>\t<nickname>\t<IP:port>\t<indicate me>\n");
    for (int index = 1; index < clients.size(); index++)
    {
        if (clients[index] != NULL)
        {
            client *c = clients[index];
            if (c->fd == fd)
            {
                printf("%d\t%s\t%s\t<-me\n", index, c->name.c_str(), c->ip.c_str());
            }
            else
            {
                printf("%d\t%s\t%s\t\n", index, c->name.c_str(), c->ip.c_str());
            }
        }
    }
}

/*
 * @return value:
 *   1 : name success
 *   -1 : name failure
 */
void name(int fd, string name)
{
    bool havSame = false;
    int clientIndex = -1;
    for (int index = 1; index < clients.size(); index++)
    {
        if (clients[index] == NULL)
            continue;
        if (clients[index]->name == name)
        {
            havSame = true;
            break;
        }
        if (clients[index]->fd == fd)
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
    clients[clientIndex]->name = name;
    broadcast(NAME, clients[clientIndex], "", 0, 0);
    return;
}

void broadcast(BROADCAST_TYPE type, client *from, string msg, int targetFD, int targetIndex)
{
    char buf[BUFSIZE];
    memset(buf, 0, sizeof(char) * BUFSIZE);

    int indexFrom, indexTarget;
    for (int i = 1; i < clients.size(); i++)
    {
        if (clients[i] == NULL)
            continue;
        if (from->fd == clients[i]->fd)
            indexFrom = i;
        if (clients[i]->fd == targetFD)
            indexTarget = i;
    }
    switch (type)
    {
    case LOGIN:
        sprintf(buf, "*** User '%s' entered from %s. ***\n", from->name.c_str(), from->ip.c_str());
        break;
    case LOGOUT:
        sprintf(buf, "*** User '%s' left. ***\n", from->name.c_str());
        break;
    case NAME:
        sprintf(buf, "*** User from %s is named '%s'. ***\n", from->ip.c_str(), from->name.c_str());
        break;
    case YELL:
        sprintf(buf, "*** %s yelled ***: %s\n", from->name.c_str(), msg.c_str());
        break;
    case TELL:
        sprintf(buf, "*** %s told you ***: %s\n", from->name.c_str(), msg.c_str());
        break;
    case SEND:
        sprintf(buf, "*** %s (#%d) just piped '%s' to %s (#%d) ***\n", from->name.c_str(), indexFrom, msg.c_str(), clients[indexTarget]->name.c_str(), indexTarget);
        break;
    case RECV:
        sprintf(buf, "*** %s (#%d) just received from %s (#%d) by '%s' ***\n", from->name.c_str(), indexFrom, clients[indexTarget]->name.c_str(), indexTarget, msg.c_str());
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
    default:
        break;
    }

    string output(buf);
    if (type == TELL)
    {
        if (send(targetFD, output.c_str(), output.size(), 0) < 0)
        {
            perror("tell/send");
        }
        return;
    }
    if (type == ERROR_USER || type == ERROR_PIPE_NOT_EXIST || type == ERROR_PIPE_IS_EXIST)
    {
        if (send(from->fd, output.c_str(), output.size(), 0) < 0)
        {
            perror("tell/send");
        }
        return;
    }
    for (int fd = 0; fd < nfds; fd++)
        if (fd != msock && FD_ISSET(fd, &afds))
            if (send(fd, output.c_str(), output.size(), 0) < 0)
                perror("broadcast/send");
}

char *printEnv(string variable, client *c)
{
    if (c->environment.count(variable))
        return (char *)(c->environment[variable].c_str());

    if (getenv(variable.c_str()) != NULL)
        return getenv(variable.c_str());
}
int setEnv(string s1, string s2, client *c)
{
    c->environment[s1] = s2;
    if (setenv(s1.c_str(), s2.c_str(), 1) == -1)
    {
        cerr << "setenv error" << endl;
        return (-1);
    };

    return 1;
}
