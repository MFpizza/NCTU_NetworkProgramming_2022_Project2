#include <iostream>
#include <string.h>
#include <vector>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
using namespace std;

struct myNumberPipe
{
    int number; // Next number time to pipe the output
    int numberPipe[2];
};
vector<myNumberPipe> number_pipe_array;

struct myCommandLine
{
    vector<string> inputCommand; // store the command line
    bool numberPipe = false;     // true if there is a number pipe command
    int numberPipeIndex = -1;    // 還在思考要用甚麼方式來儲存numberPipe
    bool errPipeNeed = false;    // true if there is a pipe command be
    int *pipeTo = NULL;          // index of pipe To
    int *pipeFrom = NULL;        // index of pipe From
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

    string s;
    cout << "% ";
    while (getline(cin, s))
    {
        if (s == "")
        {
            cout << "% ";
            continue;
        }

        // * splite string with spaces
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
    int parseCommandLine = 0;
    vector<myCommandLine> parseCommand;
    parseCommand.resize(1);
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

            //* normal pipe
            else if (count != SeperateInput.size() - 1)
            {
                myCommandLine newCommand;
                parseCommand.push_back(newCommand);
                parseCommandLine++;
            }
        }
        else
            parseCommand[parseCommandLine].inputCommand.push_back(SeperateInput[count]);
    }

    int pipeArray[2][2];
    for (int i = 0; i < parseCommand.size(); i++)
    {
        // * front Pipe
        if (i > 0)
            parseCommand[i].pipeFrom = pipeArray[(i - 1) % 2];

        // * back Pipe
        if (i != parseCommand.size() - 1)
        {
            parseCommand[i].pipeTo = pipeArray[i % 2];
            if (pipe(pipeArray[i % 2]) < 0)
                perror("pipe gen failed");
        }

        //  * handle numberPipe stdIn
        if (i == 0 && indexInNumberPipe != -1)
            parseCommand[i].pipeFrom = number_pipe_array[indexInNumberPipe].numberPipe;

        // * number Pipe behind
        if (parseCommand[i].numberPipe)
            parseCommand[i].pipeTo = number_pipe_array[parseCommand[i].numberPipeIndex].numberPipe;

        pid = fork();
        if (pid == 0) // child process
        {
            if (parseCommand[i].pipeTo != NULL)
            {
                close(parseCommand[i].pipeTo[0]);
                dup2(parseCommand[i].pipeTo[1], 1);
                if (parseCommand[i].errPipeNeed)
                    dup2(parseCommand[i].pipeTo[1], 2);
                close(parseCommand[i].pipeTo[1]);
            }

            if (parseCommand[i].pipeFrom != NULL)
            {
                close(parseCommand[i].pipeFrom[1]);
                dup2(parseCommand[i].pipeFrom[0], 0);
                close(parseCommand[i].pipeFrom[0]);
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

            if (i == 0 && indexInNumberPipe != -1)
            {
                close(number_pipe_array[indexInNumberPipe].numberPipe[0]);
                close(number_pipe_array[indexInNumberPipe].numberPipe[1]);
                number_pipe_array.erase(number_pipe_array.begin() + indexInNumberPipe);
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
    if (!parseCommand[parseCommandLine].numberPipe)
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