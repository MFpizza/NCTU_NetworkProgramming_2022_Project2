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
using namespace std;

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
};

void executeFunction(myCommandLine tag);
int parserCommand(vector<string> SeperateInput);

void signalHandler(int sig)
{
    pid_t pid = wait(NULL);
}

int shell()
{
    signal(SIGCHLD, signalHandler);
    clearenv();
    setenv("PATH", "bin:.", 1);

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

        // 分割當前的指令
        vector<string> lineSplit;
        string delimiter = " ";

        size_t pos = 0;
        string token;
        while ((pos = s.find(delimiter)) != string::npos)
        {
            token = s.substr(0, pos);
            lineSplit.push_back(token);
            s.erase(0, pos + delimiter.length());
            if (token[0] == '|' && token.size() > 1)
            {
                if (parserCommand(lineSplit) == 0)
                    return 1;
                lineSplit.clear();
            }
        }

        lineSplit.push_back(s);

        if (parserCommand(lineSplit) == 0)
            return 1;

        cout << "% ";
    }
    return 1;
}

vector<myNumberPipe> NumberPipeArray;
int GlobalPipe[1000][2];
bool GlobalPipeUsed[1000];

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
            exit(-1);
        };
        return 1;
    }
    else if (SeperateInput[0] == "exit")
    {
        return (0);
    }

    pid_t pid, wpid;
    int status = 0;

    int count = 0, parseCommandLine = 0, pipeNumber = 0;
    vector<myCommandLine> parseCommand;
    parseCommand.resize(1);

    bool hasNumberPipe = false;

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

                hasNumberPipe = true;
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

            executeFunction(parseCommand[i]);
        }
        else if (pid > 0) // parent  process
        {
            if (i > 0)
            {
                close(pipeArray[(i - 1) % 2][0]);
                close(pipeArray[(i - 1) % 2][1]);
            }

            if (i == 0 && NumberPipeNeed != -1)
            {
                close(GlobalPipe[NumberPipeNeed][0]);
                close(GlobalPipe[NumberPipeNeed][1]);
                GlobalPipeUsed[NumberPipeNeed] = false;
                NumberPipeArray.erase(NumberPipeArray.begin() + indexInNumberPipe);
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
