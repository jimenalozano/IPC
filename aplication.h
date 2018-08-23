#ifndef APLICATION_H
#define APLICATION_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <dirent.h>
#include <semaphore.h>
#include <stddef.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/select.h>

#include "slaveProtocol.h"
#include "viewProtocol.h"

#define N_CHLDRN 10
#define PERCENTAGE 0.40
#define PIPE 2
#define BUFFER_SIZE 10000
#define MAX_INTENTOS 10

typedef struct doubleNode {
    char buffer[BUFFER_SIZE];
    off_t size;
    struct doubleNode * prev;
    struct doubleNode * next;
} doubleNode;

typedef doubleNode * doubleNodePtr;

typedef struct child {
    pid_t pid;
    struct child * next;
} child;

typedef child * childPtr;

childPtr createChildren();
doubleNodePtr processFiles(int argc, char* argv[]);
void closePipes();
int isDirectory(const char *);
off_t getSize(const char *);
void setBuffer(char *, const char *);
doubleNodePtr addFile(doubleNodePtr, char *);
void addHash(const char [BUFFER_SIZE]);
childPtr * addChild(childPtr, childPtr, pid_t);
char ** initializeData(int);
void setBufferWithFiles (char[], int);
void killChildren();
void freeResources();
void sendFiles(int, int);
void hashingFiles();
void signalInitSegFault();
void segFaultHandler(int, siginfo_t *, void *);
void cntrlCHandler();
void saveDataToFile();
void viewInitHandler(int, siginfo_t *, void *);
bool initIpcs();
void errorIpcs();
void closeIpcs();
void cleanExit();
void saveDataToBuffer(const char[BUFFER_SIZE]);
void saveWithoutIpcs(const char[BUFFER_SIZE]);
void saveWithIpcs(const char[BUFFER_SIZE]);
void saveDataToIpcsBuffer(const char[BUFFER_SIZE]);
void switchSaveFunction(void (*)(const char *));
void setViewInitHandler();
void notifyExitToView();
void saveInShm(const char[BUFFER_SIZE]);
void waitForView();
void printPid();


#endif

