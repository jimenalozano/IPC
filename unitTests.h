#ifndef UNITTESTS_H
#define UNITTESTS_H

#include <sys/stat.h>
#include <assert.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define TRUE 1
#define FALSE 0
#define BUFFER_SIZE 1000
#define PATH 0
#define MD5 1

char * hasFile(void);
int checkMD5(char * fileName);
int isFile(char * fileName);
void calculateMD5(char path[], char md5[32]);

#endif
