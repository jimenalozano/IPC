#ifndef VIEW_H
#define VIEW_H

#include "viewProtocol.h"

void notifyApp(pid_t);
void initIpcs();
void closeIpcs(void);
void appEndHandler(int);
void cleanExit(void);
void NonStopBufferReading();
void readBuffer();
void printData(char[MAX_CHAR_PASSAGE]);
void setAppEndHandler();

#endif
