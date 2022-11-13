#include <iostream>
#include <string.h>
#include <vector>
#include <unistd.h>
#include <filesystem>
#include <stdlib.h>
#include <signal.h>
#include <vector>
#include <fcntl.h>
#include <fstream>
#include <pwd.h>
#include <algorithm>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <arpa/inet.h>
#include <map>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <dirent.h>
using namespace std;
#define LISTEN_BACKLOG 50
#define QLEN 5
#define BUFSIZE 4096
#define MAX_CLIENT 31
#define MAX_BROADCAST 10
#define USERPIPE_PATH "user_pipe/"
#define SM_PATH "shared_memory/"

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

struct client
{
    int fd = -1;
    char name[40];
    char ip[INET6_ADDRSTRLEN];
    int pid = -1;
    bool used = false;
};

struct broadcastMsg
{
    char msg[BUFSIZE];
    bool used = false;
    int toFD = 0; // 0 = broadcast other is client index
};

int userPipeFDArray[31] = {0};
vector<myNumberPipe> NumberPipeArray;
int GlobalPipe[1000][2];
bool GlobalPipeUsed[1000];

int msock; // master socket fd
int ppid;
int myIndex;
client *me;

// shared memory
int clients_shared_momory_fd;
int broadcast_shared_momory_fd;
client *clients;
broadcastMsg *BM;

// 格式 本身自己的fd : 對方的fd 要傳入自己本身的對應的pipe

void executeFunction(myCommandLine tag);
int parserCommand(vector<string> SeperateInput);
void broadcast(BROADCAST_TYPE type, int fromFD, string msg, int targetFD, int targetIndex);
void ServerBroadcast();
void who();
void name(string name);
void logoutControl(int pid);
char *printEnv(string variable);
int setEnv(string s1, string s2);
void printClients(int i);
void signalHandler(int sig);