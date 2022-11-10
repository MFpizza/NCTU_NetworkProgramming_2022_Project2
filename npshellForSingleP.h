#include <iostream>
#include <string.h>
#include <vector>
#include <sys/wait.h>
#include <unistd.h>
#include <filesystem>
#include <stdlib.h>
#include <signal.h>
#include <sys/stat.h>
#include <vector>
#include <fcntl.h>
#include <fstream>
#include <pwd.h>
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
} BROADCAST_TYPE;

struct myNumberPipe
{
    int number;            // Next number time to pipe the output
    int IndexOfGlobalPipe; // Index of global pipe
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
    int fd;
    string name;
    string ip;
    vector<myNumberPipe> NumberPipeArray;
    int GlobalPipe[1000][2];
    bool GlobalPipeUsed[1000];
};

int msock; // master socket fd
int nfds;
fd_set rfds;
fd_set afds;
vector<client> clients;

map<int, map<int, int *>> userPipe;
// 格式 本身自己的fd : 對方的fd 要傳入自己本身的對應的pipe

void executeFunction(myCommandLine tag, int fd);
int parserCommand(vector<string> SeperateInput, int fd, client *c);
void broadcast(BROADCAST_TYPE type, client *from, string msg, int toFD);
void who(int fd);
void name(int fd, string name);
void logoutControl(int fd);

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
        if (clients[i].fd == fd)
            c = &clients[i];
    }

    dup2(fd, 1);
    dup2(fd, 2);

    signal(SIGCHLD, signalHandler);
    clearenv();
    setenv("PATH", "bin:.", 1);

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

int findTheGlobalPipeCanUse(client *c)
{
    for (int i = 0; i < 1000; i++)
    {
        if (!c->GlobalPipeUsed[i])
        {
            c->GlobalPipeUsed[i] = !c->GlobalPipeUsed[i];

            if (pipe(c->GlobalPipe[i]) == -1)
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
int parserCommand(vector<string> SeperateInput, int fd, client *c)
{
    int rc;
    int NumberPipeNeed = -1, indexInNumberPipe = -1;
    for (int j = 0; j < c->NumberPipeArray.size(); j++)
    {
        c->NumberPipeArray[j].number = c->NumberPipeArray[j].number - 1;
        if (c->NumberPipeArray[j].number == 0)
        {
            indexInNumberPipe = j;
            NumberPipeNeed = c->NumberPipeArray[j].IndexOfGlobalPipe;
        }
    }

    if (SeperateInput[0] == "printenv")
    {
        if (getenv(SeperateInput[1].c_str()) != NULL)
        {
            printf("%s\n", getenv(SeperateInput[1].c_str()));
        }
        return 1;
    }
    else if (SeperateInput[0] == "setenv")
    {
        if (setenv(SeperateInput[1].c_str(), SeperateInput[2].c_str(), 1) == -1)
        {
            cerr << "setenv error" << endl;
            return (-1);
        };
        return 1;
    }
    else if (SeperateInput[0] == "exit")
    {
        return 0;
    }
    else if (SeperateInput[0] == "name" )
    {
        if(SeperateInput.size() > 2)
        {
            cout<<"only one word behind name: [name id]"<<endl;
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

        broadcast(YELL, c, msg, 0);
        return 1;

        cerr << "no found client\n";
        return -1;
    }
    else if (SeperateInput[0] == "tell")
    {
        int index = stoi(SeperateInput[1]);
        if (index < 1 && index > clients.size())
        {
            cout << "*** Error: user #" << index << " does not exist yet. ***\n";
            return 1;
        }
        string msg = "";
        for (int i = 2; i < SeperateInput.size(); i++)
            if (i != 2)
                msg += (" " + SeperateInput[i]);
            else
                msg += SeperateInput[i];

        broadcast(TELL, c, msg, clients[index].fd);
        return 1;
    }

    pid_t pid, wpid;
    int status = 0;

    int count = 0, parseCommandLine = 0, pipeNumber = 0;
    vector<myCommandLine> parseCommand;
    parseCommand.resize(1);

    bool hasNumberPipe = false;

    int myIndex;
    for (int clientIndex = 1; clientIndex < clients.size(); ++clientIndex)
    {
        if (clients[clientIndex].fd == fd)
        {
            myIndex = clientIndex;
        }
    }

    while (count < SeperateInput.size())
    {
        //! 若是userpipe出error 該繼續運行後面的指令還是直接返回
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

                hasNumberPipe = true;
                bool hasPipe = false;
                parseCommand[parseCommandLine].numberPipe = true;

                for (int k = 0; k < c->NumberPipeArray.size(); k++)
                {
                    if (c->NumberPipeArray[k].number == Number)
                    {
                        hasPipe = true;
                        parseCommand[parseCommandLine].numberPipeIndex = c->NumberPipeArray[k].IndexOfGlobalPipe;
                    }
                }

                if (!hasPipe)
                {
                    int GlobalPipeIndex = findTheGlobalPipeCanUse(c);
                    myNumberPipe nP;
                    nP.number = Number;
                    nP.IndexOfGlobalPipe = GlobalPipeIndex;
                    c->NumberPipeArray.push_back(nP);
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

            count++;
            if (count == SeperateInput.size())
                break;
        }

        // user pipe
        if (SeperateInput[count][0] == '<' || SeperateInput[count][0] == '>')
        {
            char userPipeChar = SeperateInput[count][0];
            SeperateInput[count].erase(SeperateInput[count].begin());
            int targetIndex = atoi(SeperateInput[count].c_str());

            if (targetIndex == myIndex)
            {
                cout << "can not pass to yourself" << endl;
                return 1;
            }

            if (targetIndex > clients.size() || targetIndex <= 0)
            {
                cout << " *** Error: user #" << targetIndex << " does not exist yet. ***" << endl;
                return 1;
            }
            // targetIndex 是 對方的clients index not FD Number;

            string msg = "";
            for (int i = 0; i < parseCommand[parseCommandLine].inputCommand.size(); i++)
            {
                msg += (parseCommand[parseCommandLine].inputCommand[i] + " ");
            }
            msg += ((userPipeChar) + SeperateInput[count]);

            int from, to;
            if (userPipeChar == '<')
            { // 接收
                to = myIndex;
                from = targetIndex;
                if (userPipe.count(to) && userPipe[to].count(from))
                {
                    //* 存在pipe 可以儲存pipeInfrom
                    parseCommand[parseCommandLine].pipeFrom = from;
                    broadcast(RECV, c, msg, clients[from].fd);
                }
                else
                {
                    //* 不存在pipe
                    cout << " *** Error: the pipe #" << from << "->#" << to << " does not exist yet. ***" << endl;
                    return 1;
                }
            }
            else
            { // 傳出
                to = targetIndex;
                from = myIndex;
                // TODO: 1.創建對方的map 2. 創建對方對應自己的map 3. 建立pipe
                //* 1. 確定對方的map是否存在 存在進2. 不存在將其建立起來
                if (!userPipe.count(to))
                {
                    map<int, int *> newChildMap;
                    userPipe[to] = newChildMap;
                }

                //* 2. 確認對方對於自己的pipe是否存在 存在表示前面pipe過東西對方還沒接收 不存在即可存進3.
                if (userPipe[to].count(from))
                {
                    cout << " *** Error: the pipe #" << from << "->#" << to << " already exists. ***" << endl;
                    return 1;
                }

                //* 3. 建立pipe並儲存pipeInform到parseCommand[parseCommandLine]
                userPipe[to][from] = new int[2]();
                if (pipe(userPipe[to][from]) < 0)
                    perror("userPipe error");

                parseCommand[parseCommandLine].pipeTo = to;
                broadcast(SEND, c, msg, clients[to].fd);
            }

            if (count != SeperateInput.size() - 1)
            {
                myCommandLine newCommand;
                parseCommand.push_back(newCommand);
                parseCommandLine++;
            }
            count++;
            if (count == SeperateInput.size())
                break;
        }

        parseCommand[parseCommandLine].inputCommand.push_back(SeperateInput[count]);
        count++;
    }

    int pipeArray[2][2];
    for (int i = 0; i < parseCommand.size(); i++)
    {
        if (pipe(pipeArray[i % 2]) < 0)
            perror("pipe gen failed");

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
                    dup2(c->GlobalPipe[i % 2][1], 2);
                }
                close(pipeArray[i % 2][1]);
            }

            // * number Pipe behind
            if (parseCommand[i].numberPipe)
            {
                // cerr<<"numberpipe"<<endl;
                int NumberPipeIndex = parseCommand[i].numberPipeIndex;
                close(c->GlobalPipe[NumberPipeIndex][0]);
                dup2(c->GlobalPipe[NumberPipeIndex][1], 1);
                if (parseCommand[i].errPipeNeed)
                {
                    dup2(c->GlobalPipe[NumberPipeIndex][1], 2);
                }
                close(c->GlobalPipe[NumberPipeIndex][1]);
            }

            //  * handle numberPipe stdIn
            if (i == 0 && NumberPipeNeed != -1)
            {
                close(c->GlobalPipe[NumberPipeNeed][1]);
                dup2(c->GlobalPipe[NumberPipeNeed][0], 0);
                close(c->GlobalPipe[NumberPipeNeed][0]);
            }

            // * handle userPipe in
            if (parseCommand[i].pipeFrom != -1)
            {
                int *ptr = userPipe[myIndex][parseCommand[i].pipeFrom];
                close(ptr[1]);
                dup2(ptr[0], 0);
                close(ptr[0]);
            }

            // * handle userPipe Out
            if (parseCommand[i].pipeTo != -1)
            {
                int *ptr = userPipe[parseCommand[i].pipeTo][myIndex];
                close(ptr[0]);
                dup2(ptr[1], 1);
                close(ptr[1]);
            }

            executeFunction(parseCommand[i], fd);
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
                close(c->GlobalPipe[NumberPipeNeed][0]);
                close(c->GlobalPipe[NumberPipeNeed][1]);
                c->GlobalPipeUsed[NumberPipeNeed] = false;
                c->NumberPipeArray.erase(c->NumberPipeArray.begin() + indexInNumberPipe);
            }

            // * close userpipe
            if (parseCommand[i].pipeFrom != -1)
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

    if (!hasNumberPipe)
    {
        while ((wpid = wait(&status)) > 0)
        {
        };
    }

    return 1;
}

void executeFunction(myCommandLine tag, int fd)
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

void who(int fd)
{
    printf("<ID>\t<nickname>\t<IP:port>\t<indicate me>\n");
    for (int index = 1; index < clients.size(); index++)
    {
        client c = clients[index];
        if (c.fd == fd)
        {
            printf("%d\t%s\t%s\t<-me\n", index, c.name.c_str(), c.ip.c_str());
        }
        else
        {
            printf("%d\t%s\t%s\t\n", index, c.name.c_str(), c.ip.c_str());
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
        if (clients[index].name == name)
        {
            havSame = true;
            break;
        }
        if (clients[index].fd == fd)
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
    clients[clientIndex].name = name;
    broadcast(NAME, &clients[clientIndex], "", 0);
    return;
}

void broadcast(BROADCAST_TYPE type, client *from, string msg, int targetFD)
{
    char buf[BUFSIZE];
    memset(buf, 0, sizeof(char) * BUFSIZE);

    int indexFrom, indexTarget;
    switch (type)
    {
    case LOGIN:
        sprintf(buf, "*** User '%s' entered from %s. ***\n", from->name.c_str(), from->ip.c_str());
        break;
    case LOGOUT:
        sprintf(buf, "*** User '%s' left. ***\n", from->name.c_str());
        break;
    case NAME:
        sprintf(buf, "*** User from '%s' is named '%s'. ***\n", from->ip.c_str(), from->name.c_str());
        break;
    case YELL:
        sprintf(buf, "*** %s yelled ***: %s\n", from->name.c_str(), msg.c_str());
        break;
    case TELL:
        sprintf(buf, "*** %s told you ***: %s\n", from->name.c_str(), msg.c_str());
        break;
    case SEND:
        for (int i = 1; i < clients.size(); i++)
        {
            if (from->fd == clients[i].fd)
                indexFrom = i;
            if (clients[i].fd == targetFD)
                indexTarget = i;
        }
        sprintf(buf, "*** %s (#%d) just piped '%s' to %s (#%d) ***\n", from->name.c_str(), indexFrom, msg.c_str(), clients[indexTarget].name.c_str(), indexTarget);
        break;
    case RECV:
        for (int i = 1; i < clients.size(); i++)
        {
            if (from->fd == clients[i].fd)
                indexFrom = i;
            if (clients[i].fd == targetFD)
                indexTarget = i;
        }
        sprintf(buf, "*** %s (#%d) just received from %s (#%d) by '%s' ***\n", from->name.c_str(), indexFrom, clients[indexTarget].name.c_str(), indexTarget, msg.c_str());
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
    for (int fd = 0; fd < nfds; fd++)
        if (fd != msock && FD_ISSET(fd, &afds))
            if (send(fd, output.c_str(), output.size(), 0) < 0)
                perror("broadcast/send");
}

void logoutControl(int fd)
{
    for (int i = 1; i < clients.size(); i++)
    {
        if (clients[i].fd == fd){
            clients.erase(clients.begin() + i);
            userPipe.erase(i); 
            break;   
        }
    }
}