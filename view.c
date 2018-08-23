#include "view.h"

/** Variables globales de usos multiples */
sem_t  *readSem;           /** Puntero al semaforo de lectura*/
sem_t  *writeSem;          /** Puntero al semaforo de escritura */
int shmFd;                 /** Descriptor de Memoria Compartida */
char *shmPtr;              /** Puntero a data en memoria compartida */
pid_t appPid = -1;              /** PID del proceso aplication */

int main(int argc, char **argv)
{

    /** Si me lo especifican por liena de comando tomo ese */
    if (argc == 2)
    {
        appPid = (int)strtol(argv[1], (char **)NULL, 10);
    }
    else /** Si no, tomo el del archivo */
    {
        FILE * f = fopen(PID_FILENAME,"r");
        if (f == NULL)
        {
            printf("Vista: Error: debe haber por lo menos 1 argumento o 1 archivo %s con el pid.\n\tUso: ./vista [app_pid]\n", PID_FILENAME);
            exit(EXIT_FAILURE);
        }
        fscanf(f, "%d", &appPid);
        fclose(f);
        if( appPid == -1 || kill(appPid,0) != 0) /** Si está muerto exit */
        {
            printf("Vista: Error: debe haber por lo menos 1 argumento o 1 archivo %s con el pid.\n\tUso: ./vista [app_pid]\n", PID_FILENAME);
            exit(EXIT_FAILURE);
        }
    }


    /** Leemos el pid del app para notificar que queremos ver. */
    printf("Vista: Se leyo el PID: %d\n", appPid);

    /** Notificamos y ademas esperamos que nos notifiquen.
     * Si no esperamos puede llegar a pasar que tratamos de
     * entrar a un sh o sem que no estan inicializados. */
    notifyApp(appPid);

    /** Necesitamos cerrar los IPCs en caso de exit */
    atexit(cleanExit);

    /** Preparamos los IPCs */
    initIpcs();

    /** Si la cosa viene bien seteamos un handler para cuando el app finaliza */
    setAppEndHandler();

    /** Iniciamos la lectura de la memoria compartida*/
    NonStopBufferReading();

    printf("Vista: Terminando\n");
    exit(EXIT_SUCCESS);
}

/**
 * Seteamos un handler para el caso en que el app nos avise
 * que se está apagando
 */
void setAppEndHandler()
{
    struct sigaction sa;
    sa.sa_handler = appEndHandler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(NOTIFICATION, &sa, NULL) == -1)
        printf("Vista: Error seteando el app end handler\n");

}

/**
 * Lee el buffer hasta que se reciba una señal o se muera el app
 */
void NonStopBufferReading()
{
    printf("Vista: Iniciando lectura de buffer\n");
    do
    {
        readBuffer();
    }
    while (kill(appPid, 0) == 0); /**chequeamos que siga vivo el app */
    printf("Vista: El app murió\n");
    exit(EXIT_SUCCESS);
}

/**
 * Lee el buffer una vez
 * Seteamos un tiempo maximo de espera por si el app se muere
 */
void readBuffer()
{
    static char lastRead[MAX_CHAR_PASSAGE] = "\0";
    static struct timespec timeout;
    clock_gettime(CLOCK_REALTIME, &timeout);
    timeout.tv_sec += VIEW_MAX_WAIT;
    timeout.tv_nsec = 0;
    switch (sem_timedwait(readSem,&timeout))
    {
        case 0 :
            if (strncmp(lastRead,shmPtr,MAX_CHAR_PASSAGE) != 0)
            {
                printData(shmPtr);
                strncpy(lastRead, shmPtr,MAX_CHAR_PASSAGE);
            }
            if(sem_post(writeSem) == -1)
            {
                printf("Vista: post error\n");
                exit(EXIT_FAILURE);
            }
            break;
        default:break;
    }
}

/**
 * Imprime la data con formato, asumimos que los nombres de archivos no
 * cuentan con espacios ' '
 */
void printData(char data[MAX_CHAR_PASSAGE])
{
    static int count = 1 ;
    char filename[MAX_CHAR_PASSAGE/2], md5[MAX_CHAR_PASSAGE/2];
    sscanf(data,"%s %s",filename,md5);
    printf("#%d: %s: %s\n", count++, filename, md5);
}

/**
 * Inicializacion de los IPCs.
 * En caso de error se cerrara todos los canales de comunicación y
 * se terminará el programa.
 */
void initIpcs()
{
    if((shmFd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0777)) == -1)
    {
        printf("Vista: shm_open error: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if((shmPtr = (char *) (mdHashData*)mmap(NULL, sizeof(mdHashData), PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, 0)) == MAP_FAILED)
    {
        printf("Vista: mmap error: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if((readSem = sem_open(READ_SEM_NAME,O_RDWR)) == SEM_FAILED )
    {
        printf("Vista: Init error: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    if((writeSem = sem_open(WRITE_SEM_NAME,O_RDWR)) == SEM_FAILED )
    {
        printf("Vista: Init error: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

/**
 * Encargada de cerrar todos los IPCs
 */
void closeIpcs(void)
{
    sem_unlink(READ_SEM_NAME);
    sem_unlink(WRITE_SEM_NAME);

    if(munmap(shmPtr, sizeof(mdHashData)) < 0 )
    {
        printf("Vista: munmap error\n");
    }

    shm_unlink(SHM_NAME);

    if(close(shmFd) < 0 )
    {
        printf("Vista: Close error\n");
    }
}

/**
 * Notificamos al app sobre nuestra presencia e intención
 * de leer del buffer. Luego esperamos a que el app notifique
 * de haber recibido nuestro aviso, asegurandonos que el sh y el sme
 * están habilitados por ella. Esperamos un máximo de tiempo,
 * si en ese tiempo no recibimos nada, matamos el programa
 */
void notifyApp(pid_t pid)
{

    /*
     * In  normal  usage,  the  calling  program blocks the signals in set via a
     * prior call to sigprocmask(2) (so that the default disposition  for  these
     * signals does not occur...
     * Bloqueamos la señal VISTA_NOTIFICAITON, para que no se corra el default handler
     * y de esta manera podemos esperar con sigtimewait().
     */
    sigset_t mask, oldSet;
    struct timespec timeout;
    sigemptyset (&mask);
    sigaddset (&mask, NOTIFICATION);
    timeout.tv_sec = VIEW_MAX_WAIT;
    timeout.tv_nsec = 0;

    if (sigprocmask(SIG_BLOCK, &mask, &oldSet) < 0)
    {
        printf("Vista: No se pudo blockear la señal\n");
        exit(EXIT_FAILURE);
    }

    /** notificamos, si no podemos matamos */
    printf("Vista: Enviando notificaion a pid: %d\n", pid);
    if (kill(pid, NOTIFICATION) < 0)
    {
        printf("Vista: Error al confirmar al app: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    /** si pudimos notificar, esperamos
     * con la misma mascara de señales que para el block */
    if (sigtimedwait(&mask, NULL, &timeout) < 0)
    {
        /** si hay problema entramos aca y matamos */
        if (errno == EINTR)
        {
            printf ("Vista: sigtimewait fue interrumpido por otra señal, exit\n");
        }
        else if (errno == EAGAIN)
        {
            printf ("Vista: Timeout, exit\n");
        }
        else
        {
            printf ("Vista: sigtimedwait error, exit\n");
        }
        exit(EXIT_FAILURE);
    }
    printf("Vista: Recibimos la notificacion! Los IPCs están setados y podemos empezar a leer\n");

    /** Sacamos la mascara porque no vamos a usar esta señal adelante */
    if (sigprocmask(SIG_UNBLOCK, &mask, &oldSet) < 0)
    {
        printf("Vista: No se pudo blockear la señal\n");
        exit(EXIT_FAILURE);
    }
}

/**
 * Si el App termina, queremos enterarnos, terminar de leer, cerrar los IPCs y
 * avisarle que estamos listos para cerrar lo ipcs y terminar nosotros.
 * No lo consideramos un FAILURE, por ende el programa termina con EXIT_SUCCESS.
 */
void appEndHandler(int signum)
{
    printf("Vista: se nos notificó que el app está terminando. Si es que nos queda información por imprimir, vamos a intentar leerla, luego exit.\n");
    /** Intentamos leer lo último que queda*/
    readBuffer();
    printf("Vista: Exit; Notificamos al App que terminamos de leer\n");
    kill(appPid, NOTIFICATION);
    exit(EXIT_SUCCESS);
}

/*
 * Al terminar, cerramos los IPCs.
 */
void cleanExit(void)
{
    /** Cerramos los IPCs */
    printf("Vista: Exit; Intentaremos cerrar los IPCs\n");
    closeIpcs();
    printf("Vista: Exit; Fin!\n");
}
