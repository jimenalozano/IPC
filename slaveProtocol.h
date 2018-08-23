#ifndef SLAVEPROTOCOL_H
#define SLAVEPROTOCOL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <sys/select.h>
#include "aplication.h"

#define HASH_SIZE 33
#define BUFFER_SIZE 10000                        /** no me esta reconociendo el BUFFER_SIZE de aplication.h */

int checkPathName(char pathNameHash[BUFFER_SIZE]);
int getPath(char pathName[BUFFER_SIZE], char buf[BUFFER_SIZE], int);

#endif
