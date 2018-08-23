/** viewProtocol.h
 * Este archivo define el protocolo de comunicación enter la aplicación y la vista.
 * Usamos named semaphore y named shared memory para la comunicación.
 * Los nombres se designan por el PID del proceso aplicación
 * Nombre del semaphore: "/sem_PID" .
 * Nombre del SH : "/sh_PID" .
 * A su ves, la información será compartida a travez de la estructura: Data.
 */


#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <signal.h>
#include <semaphore.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <stdbool.h>
#include <errno.h>
#include <memory.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define SHM_NAME "/shaal"
#define SEM_NAME "/seml"
#define READ_SEM_NAME "/readSem"
#define WRITE_SEM_NAME "/writeSem"

#define VIEW_MAX_WAIT 5
#define APP_MAX_WAIT 7
#define NOTIFICATION SIGUSR2
#define MAX_CHAR_PASSAGE 1000
#define PID_FILENAME "./appPid.txt"


typedef char mdHashData[MAX_CHAR_PASSAGE];

#endif
