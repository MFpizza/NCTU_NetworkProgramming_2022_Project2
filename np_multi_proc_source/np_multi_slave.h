#if !defined(__NP_MULTI_SLAVE_H__)
    #define __NP_MULTI_SLAVE_H__

#include <vector>
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
    ERROR_PIPE_IS_EXIST,
    SERVER
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
    int fifoToFD = -1;
    int pipeFrom = -1; // index of userpipe From
    int fifoFromFD = -1;
};

int shellwithFD(int fd);
#endif //__NP_MULTI_SLAVE_H__