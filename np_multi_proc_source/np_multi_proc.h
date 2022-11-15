#if !defined(__NP_MULTI_SERVER_H__)
    #define __NP_MULTI_SERVER_H__
    
#include <iostream>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/wait.h>
#include <sys/stat.h>
using namespace std;

#define LISTEN_BACKLOG 50
#define QLEN 5
#define BUFSIZE 4096
#define MAX_CLIENT 31
#define MAX_BROADCAST 31
#define USERPIPE_PATH string("user_pipe/")
#define SM_PATH string("user_pipe/")

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

// shared memory
extern client *clients;
extern broadcastMsg *BM;

// main server function
void printClients(int h);

#endif //__NP_MULTI_SERVER_H__