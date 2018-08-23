#include "aplication.h"

/** Variables para el manejo de la creacion de los hash*/
int size;                       /** Cantidad de archivos a procesar */
childPtr firstChild;            /** Puntero a la primera estructura de procesos hijo */
doubleNodePtr files;            /** Puntero a la primera estructura de archivos */
char ** data;                   /** Buffer interno de guardado de hashes  */
int pipefdHash[PIPE * N_CHLDRN];/** Pipes para recibo de hash */
int pipefdPath[PIPE * N_CHLDRN];/** Pipes para enviar archivos a los childs */
int numChildren;                /** Cantidad de hijos que se crearon, depende de size y N_CHLDRN_MAX  */

/** Variables para el manejo de los IPCs de la comunicación con la vista */
sem_t  *readSem;                /** Puntero al semaforo */
sem_t  *writeSem;               /** Puntero al semaforo */
int shmFd;                      /** Descriptor del file mapeado a la Memoria Compartida */
char *shmPtr ;                  /** Shared memory pointer */
pid_t viewPid = -1;             /** Flag de existencia de vista */
bool ipcsOpen = false;          /** Flag indicando si los IPCs estan abiertos */
void(*savingFunction)(const char*) = saveWithoutIpcs; /** Puntero a funcion que se encarga de guardar los hashes*/


int main (int argc, char* argv[])
{
    /** Imprimimos el PID como lo sugiere la cátedra
     * y ademas lo mandamos a un archivo para que lo lea
     * la vista y no lo tenga que estar tipeando
     */
    printPid();

    /** Seteamos un handler para el caso en que aparezca una vista */
    setViewInitHandler();

    /** Dormimos como sugiere la catedra para esperar a la vista */
    waitForView();

    /** cuando termine el programa, los esclavos mueren y se libera el espacio
     * ocupado por ellos y por los files
     * y liberamos los IPCs
     */
    atexit(cleanExit);

    /** copiamos las variables globales para usarlas desde otras funciones */
    size = argc-1;

    /** inicializo la señal del seg fault */
    signalInitSegFault();

    /** inicializo la señal del control c */
    signal(SIGINT, (void (*)(int)) cntrlCHandler);

    /** inicializo la señal que me indica si un proceso hijo se murio */
    signal(SIGCHLD, (void (*)(int)) cleanExit);

    /** ordeno los archivos que me pasaron como parametro segun su tamaño */
    files = processFiles(argc, argv);
    if (files == NULL)
    {
        exit(EXIT_FAILURE);
    }

    /** inicializo la estructura que voy a actualizar a medida que me lleguen los hashes */
    data = initializeData(size);

    numChildren = (size < N_CHLDRN) ? size : N_CHLDRN;

    /** creo a los hijos */
    firstChild  = createChildren();
    if (firstChild == NULL)
    {
        exit(EXIT_FAILURE);
    }

    /** hago el hashing de los files */
    hashingFiles();

    /** cuando ya no tengo mas archivos, cierro los pipes */
    closePipes();

    /** Guardamos nuestro buffer en un archivo como lo pide la catedra*/
    saveDataToFile();

    exit(EXIT_SUCCESS);
}

void printPid()
{
    FILE * f = fopen(PID_FILENAME,"w");
    fprintf(f,"%d",getpid());
    fclose(f);
    printf("App: PID: %d\n", getpid());
}

void waitForView()
{
    printf("App: durmiendo %d segundos para esperar que alguna vista se conecte\n", APP_MAX_WAIT);
    int i = 0;
    while (i < APP_MAX_WAIT && viewPid == -1)
    {
        sleep(1);
        i++;
    }
}

void saveDataToFile()
{
    char filename[30];
    sprintf(filename,"./md5hash_%lu.txt",(unsigned long)time(NULL));
    FILE *f = fopen(filename,"w");
    if (f == NULL)
    {
        printf("App: No se pudo abrir el archivo %s", filename);
        exit(EXIT_FAILURE);
    }
    int i = 0;
    while (i < size && data[i])
    {
        fprintf(f,"%s\n", data[i++]);
    }
    fclose(f);
    printf("App: Los hash fueron guardadon en el archivo %s\n", filename);
}

/** crea los procesos esclavos, setea los pipes y arma una lista con sus pid's
 * si hay algun error, retorna NULL
 */
childPtr createChildren()
{
    childPtr first = NULL;
    childPtr last = NULL;
    int i, counter = 0;
    int numFiles = (int) (size * PERCENTAGE);
    int numFilesPerChild = numFiles/N_CHLDRN;
    if (numFilesPerChild == 0)
    {
        numFilesPerChild=1;
    }

    for (i=0; i<numChildren; i++)
    {
        if (pipe(&pipefdPath[i*2]) < 0|| pipe(&pipefdHash[i*2]) < 0)
        {
            fprintf(stderr,"No se pudo abrir los pipes: %s\n\n", strerror(errno));
            exit(EXIT_FAILURE);
        }

        pid_t chld = fork();

        childPtr * result;
        result = addChild(first, last, chld);

        if(result != NULL)          /** entonces no fue el hijo el que hizo el addChild */
        {
            first = result[0];
            last = result[1];
            free(result);
        }

        if (chld == 0)
        {
            sleep(3);                               /** para que coordinen el padre y sus hijos; ver si funciona sin sleeps */
            dup2(pipefdPath[i*2], fileno(stdin));
            dup2(pipefdHash[i*2 + 1], fileno(stdout));
            close(pipefdPath[i*2 + 1]);
            close(pipefdHash[i*2]);

            if (execl("./slaves.o","./slaves.o", NULL) != 0)
            {
                printf("Error: children execl\n");
                counter--;
            }
        }
        else if (chld > 0)
        {
            close(pipefdPath[i*2]);
            close(pipefdHash[2*i + 1]);
            sendFiles(i, numFilesPerChild);
            counter++;
        }
        else
        {
            printf("Error: creating childPtr with PID: %d\n", chld);
            counter--;
        }
    }

    if (last != NULL)
    {
        last->next = NULL;
    }

    if (counter != numChildren)
    {
        return NULL;
    }

    return first;
}

/** agrega el hijo con el pid en la lista de hijos
 * retorno un puntero al primer y ultimo hijo porque cuando termina la funcion esos valores cambian
 */
childPtr * addChild(childPtr first, childPtr last, pid_t pid)
{
    if(pid == 0)
    {
        return NULL;
    }
    childPtr newChild = malloc(sizeof(childPtr));
    newChild->pid=pid;
    newChild->next=NULL;

    if (first == NULL)
    {
        first = newChild;
        last = newChild;
    }
    else
    {
        last->next = newChild;
        last = newChild;
    }

    childPtr * result = malloc(2*sizeof(childPtr));
    result[0] = first;
    result[1] = last;
    return result;
}

/** manda archivos cuando crea hijos para distribuir al comienzo
 * si tengo solo 2 archivos para hashear, no hace falta crear mas de 2 hijos  */
void sendFiles(int posChild, int numFilesPerChild)
{
    char buffer[BUFFER_SIZE];

    setBufferWithFiles(buffer, numFilesPerChild);

    if (write(pipefdPath[posChild*2+1], buffer, BUFFER_SIZE) < 0)
    {
        printf("Error: writing to children\n");
    }
}

/** lee los argumentos, arma los paths, y arma una lista de archivos */
doubleNodePtr processFiles(int argc, char * argv[])
{
    if (argc == 1)
    {
        printf("App: Error, es necesario indicar al menos 1 archivo. Uso: ./aplication.c [archivo.txt] ...\n");
        return NULL;
    }

    doubleNodePtr first = NULL;

    int i;
    for (i=1; i<argc; i++)
    {
        char path[BUFFER_SIZE];
        setBuffer(path, argv[i]);
        if (isDirectory(path))
        {
            size--;
        }
        else
        {
            first = addFile(first, path);
        }
    }

    return first;
}

/** agrego el archivo a la lista de archivos
 * su posicion en la lista depende de su tamaño
 */
doubleNodePtr addFile(doubleNodePtr first, char * path)
{
    doubleNodePtr elem = malloc(sizeof(doubleNode));
    elem->size = getSize(path);
    memcpy(elem->buffer, path, BUFFER_SIZE);

    if (first == NULL)
    {
        elem->next=NULL;
        elem->prev=NULL;
        first = elem;
    }

    else
    {
        doubleNodePtr aux = first;
        bool found = false;
        while (aux->next!=NULL && !found)
        {
            if (aux->size >= elem->size)
            {
                elem->next=aux;
                elem->prev=aux->prev;
                if (aux->prev != NULL)
                {
                    aux->prev->next=elem;
                }
                else
                {
                    first=elem;
                }
                aux->prev=elem;
                found=true;
            }
            else
            {
                aux=aux->next;
            }
        }

        if (!found)
        {
            if(aux->size < elem->size)
            {
                aux->next=elem;
                elem->prev=aux;
                elem->next=NULL;
            }
            else
            {
                elem->next=aux;
                elem->prev=aux->prev;
                if(aux->prev != NULL)
                {
                    aux->prev->next=elem;
                }
                else
                {
                    first=elem;
                }
                aux->prev=elem;
            }

        }
    }
    return first;
}

/** cierra los pipes de los hijos, y el pipe general */
void closePipes()
{
    int i;
    for(i=0; i < N_CHLDRN; i++)
    {
        close(pipefdPath[2*i]);
        close(pipefdHash[2*i + 1]);
    }
}

/** retorna si el archivo ubicado en el path es un directorio */
int isDirectory(const char * path)
{
    struct stat statbuf;
    if (stat(path, &statbuf) != 0)
    {
        return 0;
    }
    return S_ISDIR(statbuf.st_mode);
}

/** retorna el tamaño del archivo ubicado en el path
 * chequear que ande bien, porque le paso un path con el formato del buffer*/
off_t getSize(const char * path)
{
    struct stat st;
    stat(path, &st);
    return st.st_size;
}

/** arma el formato del buffer de un solo archivo */
void setBuffer(char * buffer, const char * fileName)
{
    int i, start;                        /** start me indica desde que posicion del buffer copio el path */
    size_t length = strlen(fileName);
    if (fileName[0] == '.')      /** ya me pasaron el path */
    {
        start = 0;
    }
    else if( fileName[0] == '/')
    {
        buffer[0] = '.';
        start = 1;
    }
    else
    {
        buffer[0]='.';
        buffer[1]='/';
        start = 2;
    }
    for (i=start; i<length+start; i++)
    {
        buffer[i] = fileName[i-start];
    }
    buffer[i] = '\0';
}

/**
 * Se encarga de llamar a una de las 2 funciones que
 * guardan los hashes.
 * Se armó 2 funciones disintas para no tener que estar
 * preguntando en cada iteración si hay ipcs abiertos entre el app
 * y la vista.
 * En caso de haber uan vista se cambia la funcion de agregado
 */
void addHash(const char buffer[BUFFER_SIZE-1])
{
    (*savingFunction)(buffer);
}

/** inicializo el array global "data" donde guardo los buffers de cada file */
char ** initializeData (int size)
{
    char ** data = malloc(size * sizeof(char*));
    int i, length;
    doubleNodePtr first = files;
    for (i=0; i<size; i++)
    {
        data[i] = malloc(BUFFER_SIZE);
        if(first != NULL)
        {
            length = (int)strlen(first->buffer);
            int j;
            for (j = 0; j < length; j++)
            {
                data[i][j] = first->buffer[j];
            }
            data[i][j] = '\0';
            first = first->next;
        }
    }
    return data;
}

/** armo el buffer que le paso a los esclavos:
 *  formato: [ #archivos, "nombre_de_archivo\0", ... (asi con todos los files)]
 */
void setBufferWithFiles (char buffer[], int numFiles)
{
    int i;
    int length = 0;      /** ---> CPP check quiere esta variable declarada adentro del for pero no nos pareció una mejora */
    int totalSize = 1;
    doubleNodePtr prevNode;

    for (i=0; i<numFiles && files!=NULL; i++)
    {
        length = (int) strlen(files->buffer);

        /** si el archivo que quiero agregar al buffer va a ser que el tamaño total se exceda de BUFFER_SIZE
         * va a causar un seg fault
         * entonces no lo agrego, lo dejo en files para el proximo child
         */
        if (length + totalSize >= BUFFER_SIZE)
        {
            numFiles--;
            buffer[totalSize] = '\0';
        }
        else
        {
            int j;
            for(j = 0; j < length; j++)
            {
                buffer[totalSize + j] = files->buffer[j];

            }
            buffer[totalSize + length] = '\0';
            totalSize += length+1;

            /** asignamos prevNode por si file->next == null */
            prevNode = files;
            files = files->next;
            free(prevNode);
        }
    }
    buffer[0] = (char) numFiles;
    for (i = totalSize; i < BUFFER_SIZE; i++)
    {
        buffer[i] = 0;
    }
}

/** procesa los archivos con sus hash, numChildren no necesariamente es N_CHLDRN
 * (si me pasaron menos de N_CHLDRN) */
void hashingFiles()
{
    /** creo un reader para select */
    fd_set readfd;
    int numberSlaves = (size>=numChildren)?numChildren:size;
    while(numberSlaves > 0)
    {
        /** reseteo el readfd para usar select en cada while */
        FD_ZERO(&readfd);
        int j;
        for(j = 0; j < numChildren; j++)
        {
            FD_SET(pipefdHash[2*j], &readfd);
        }

        int available = select(pipefdHash[2 * numChildren - 1]+1, &readfd, NULL, NULL, NULL);
        if(available == -1)
        {
            fprintf(stderr,"Error: select: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }

        for(j = 0; j < numChildren && available; j++)
        {
            if(FD_ISSET(pipefdHash[2*j], &readfd))
            {
                char hash[BUFFER_SIZE];
                if (read(pipefdHash[j*2], hash, BUFFER_SIZE) == -1)
                {
                    printf("Error: reading pipe\n");
                    exit(EXIT_FAILURE);
                }
                if(files == NULL)
                {
                    numberSlaves--;
                }
                else if (hash[0] < 1)
                {
                    char fileBuffer[BUFFER_SIZE];
                    setBufferWithFiles(fileBuffer, 1);
                    if(write(pipefdPath[2 * j + 1], fileBuffer, BUFFER_SIZE) == -1)
                    {
                        printf("Error: writing pipe\n");
                        exit(EXIT_FAILURE);
                    }
                }
                available--;
                if(*(hash) == (char)255)
                {
                    printf("%s\n", hash+1);
                }
                else
                {
                    addHash(hash+1);
                }
            }
        }
    }
}

/** Setea el handler por si aparece la vista */
void setViewInitHandler()
{
    struct sigaction sa;
    sa.sa_sigaction = viewInitHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO;
    if (sigaction(NOTIFICATION, &sa, NULL) == -1)
        printf("App: Error en el handler\n");
}

/**
 * Se encarga de guardar información en el caso que NO
 * aparesca una vista
 */
void saveWithoutIpcs(const char buffer[BUFFER_SIZE])
{
    saveDataToBuffer(buffer);
}

/**
 * Se encarga de guardar información en el caso que SI
 * aparesca una vista
 */
void saveWithIpcs(const char buffer[BUFFER_SIZE])
{
    /** guardamos la informacion en nuestro buffer */
    saveDataToBuffer(buffer);

    /** compartimos con los ipcs */
    saveInShm(buffer);

}

/**
 * Se encarga de guardar información en el nuestro
 * buffer interno Data
 */
void saveDataToBuffer(const char buffer[BUFFER_SIZE])
{
    int length = (int) strlen(buffer);
    int i, pos = 0;
    bool found = false;
    for (i=length-1; i>=0 && !found; i--)
    {
        if (buffer[i] == ' ')
        {
            pos = i;
            found = true;
        }
    }
    char * name = malloc(sizeof(char)*BUFFER_SIZE);
    memcpy(name, buffer, BUFFER_SIZE);
    name[pos] = '\0';
    int lengthAux1 = (int) strlen(name);
    found = false;

    for (i =0; i<size && !found; i++)
    {
        int lengthAux2 = (int) strlen(data[i]);

        /** comparo lengthAux1 (longitud del nombre del hash por agregar)
         * con lengthAux2 (longitud del nombre del archivo en la posicion i de data) */
        if (lengthAux1 == lengthAux2 && strcmp(name, data[i]) == 0)
        {
            memcpy(data[i], buffer, BUFFER_SIZE);
            found = true;
        }
    }

    if (!found)
    {
        printf("Error: adding hash to data \n");
    }

    free(name);
}

/**
 * Encargada de guardar información en buffer
 * de memoria compartida para enviar a la vista.
 * No queremos perder tiempo con los semaforos,
 * por ende, si la vista no entrega el semaforo en APP_MAX_WAIT segundos,
 * le damos MAX_INTENTOS que puede repetir este comportamiento, sino matamos
 * la comunicacion entre las dos partes.
 * En una corrida de hash de miles de archivos esto podría enlentecer demasiado.
 */
void saveInShm(const char buffer[BUFFER_SIZE])
{
    static int intentos = 0;
    static struct timespec timeout;
    clock_gettime(CLOCK_REALTIME, &timeout);
    timeout.tv_sec += APP_MAX_WAIT;
    timeout.tv_nsec = 0;

    /** si estan cerrados o esta muerta o bloqueada.. */
    if (!ipcsOpen || kill(viewPid, 0) != 0 || intentos >= MAX_INTENTOS)
    {
        printf("App: Parece que la vista murió o se colgó, mataremos la comunicación entre las dos partes\n");
        closeIpcs();
        switchSaveFunction(saveWithoutIpcs);
    }
    else if(ipcsOpen && sem_timedwait(writeSem,&timeout) == 0)
    {
        saveDataToIpcsBuffer(buffer);
        sem_post(readSem);
        intentos = 0;
        return;
    }
    else /** no me lo dió enseguida.. */
    {
        printf("App: no entregó el semaforo enseguida\n");
        intentos ++;
    }
}

/**
 * Encargada de cambiar la función de gaurdado
 * de información.
 * Esta función es llamada en caso de que se presente/muera
 * una vista.
 */
void switchSaveFunction(void (*funcion)(const char *))
{
    savingFunction = funcion;
}

/**
 * Encargada de guardar información en la
 * memoria compartida
 */
void saveDataToIpcsBuffer(const char buffer[BUFFER_SIZE])
{
    strcpy(shmPtr,buffer);
}

/**
 * Al recibir la NOTIFICATION de una vista queriendo leer el buffer
 * el handler se encarga de llamar a la función que prepara los IPCS.
 * Una vez preparado esto, se le notifica a la vista que puede empezar a usar
 * los IPCs.
 */
void viewInitHandler(int signum, siginfo_t *info, void *ucontext)
{
    pid_t sender_pid = info->si_pid;
    printf("App: Recibimos la señal: %d, del proceso con pid: %d\n", signum, sender_pid);
    if (initIpcs() == true)
    {
        /** cambiamos la función de agregado de informacion. */
        switchSaveFunction(saveWithIpcs);
        /** notificamos el correcto funcionamiento de los IPCs */
        kill(sender_pid, NOTIFICATION);
        viewPid = sender_pid;
    }
}

/**
 * Encargada de inicializar los IPCs.
 * En caso de error no queremos matar del programa, pero cerramos
 * todos los IPCs.
 */
bool initIpcs()
{
    if (ipcsOpen)
    {
        return true;
    }
    /** Iniciamos la Memoria Compartida */
    if((shmFd = shm_open(SHM_NAME, O_CREAT | O_RDWR , 0755)) == -1)
    {
        printf("App: shm_open error: %s\n", strerror(errno));
        errorIpcs();
        return false;
    }
    /** Le damos un tamaño al file */
    if(ftruncate(shmFd, sizeof(mdHashData)) == -1)
    {
        printf("App: ftruncate error: %s\n", strerror(errno));
        errorIpcs();
        return false;
    }
    /** Mapea memoria y retorna un puntero a esa zona de memoria con permisos */
    if((shmPtr = (char *) (mdHashData*)mmap(NULL, sizeof(mdHashData) * MAX_CHAR_PASSAGE, PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, 0)) == MAP_FAILED)
    {
        printf("App: mmap error: %s\n", strerror(errno));
        errorIpcs();
        return false;
    }

    /** Ahora vamos con semaforos */
    if((readSem = sem_open(READ_SEM_NAME, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP, 0)) == SEM_FAILED )
    {
        printf("App: Init error: %s\n", strerror(errno));
        errorIpcs();
        return false;
    }
    if((writeSem = sem_open(WRITE_SEM_NAME, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP, 1)) == SEM_FAILED )
    {
        printf("App: Init error: %s\n", strerror(errno));
        errorIpcs();
        return false;
    }
    ipcsOpen = true;
    return true;
}

/**
 * Encargada de errores con los IPCs
 */
void errorIpcs()
{
    printf("App: hubo un error en la inicialización de los IPCs, estos no serán provistos, sin embargo la aplicación serguirá corriendo.\n");
    closeIpcs();
}

/**
 * Encargada de cerrar todos los IPCs, aunque ipcs Open == false
 * para asegurarnos.
 */
void closeIpcs()
{
    printf("App: Cerrando IPCs\n");

    /** Lo eliminamos, es importante hacerlo antes de desalocar la memoria donde se encuentra guardado */
    sem_unlink(READ_SEM_NAME);

    sem_unlink(WRITE_SEM_NAME);

    /** Desalocamos la memoria usada */
    if(munmap(shmPtr, sizeof(mdHashData)) < 0 )
    {
        printf("App: munmap error: %s\n", strerror(errno));
    }

    /** Cerramos la memoria compartida */
    shm_unlink(SHM_NAME);

    /** Cerramos el file descriptor de la memoria compartida */
    if(close(shmFd) < 0 )
    {
        printf("App: Close error: %s\n", strerror(errno));
    }
    ipcsOpen = false;

}

/**
 * .Al terminar, cerramos los IPCs y notificamos a la vista, si es que la hay,
 * que el programa está terminando. Esperamos a que nos responda.
 * .Matamos a los hijo
 * .Liberamos los recursos usados
 */
void cleanExit()
{
    /** Notificamos a la vista, si es que hay una y no murió */
    if (viewPid > 0 && kill(viewPid, 0) == 0)
    {
        notifyExitToView();
    }
    /** Cerramos los IPCs, si es que están abiertos */
    if (ipcsOpen)
    {
        printf("App: Exit; Intentaremos cerrar los IPCs\n");
        closeIpcs();
    }
    /** matamos los hijos */
    killChildren();
    /** liberamos recursos */
    freeResources();
    /** Eliminamos el archivo usado para pasar el pid*/
    remove(PID_FILENAME);
    printf("App: Exit; Fin!\n");
}

/**
 * Notificamos a la vista que nos estamos yendo y esperamos la respuesta
 */
void notifyExitToView()
{
    /** Creamos una mascara para recibir la señal de la Vista */
    struct timespec timeout;
    sigset_t mask, oldSet;
    sigemptyset (&mask);
    sigaddset (&mask, NOTIFICATION);

    if (sigprocmask(SIG_BLOCK, &mask, &oldSet) < 0)
    {
        printf("App: No se pudo blockear la señal\n");
    }

    printf("App: Exit; Notificamos a la vista que nos estamos yendo, esperamos una señal de respuesta dentro de los proximos %d segundos.\n", APP_MAX_WAIT);
    /** para que pueda leer lo que falta */
    sem_post(readSem);
    kill(viewPid, NOTIFICATION);

    timeout.tv_sec = APP_MAX_WAIT;
    timeout.tv_nsec = 0;
    if(sigtimedwait(&mask, NULL, &timeout) == NOTIFICATION)
    {
        printf("App: Exit; Recibimos la notificación de fin de lectura de la vista.\n");
    }
}

/** mata a los esclavos y libera los recursos
 * funcion que se invoca atexit
 */
void killChildren()
{
    freeResources();
    while (firstChild != NULL)
    {
        if (kill(firstChild->pid, SIGKILL) != 0)
        {
            printf("Error: killing child process\n");
        }
        firstChild = firstChild->next;
    }
}

/** libera los recursos de cada archivo y cada hijo
 *  cuando se consume un archivo, se lo libera en el momento
 */
void freeResources()
{
    doubleNodePtr nextFile;
    while (files != NULL)
    {
        nextFile = files->next;
        free(files);
        files = nextFile;
    }

    childPtr nextChild;
    while (firstChild != NULL)
    {
        nextChild = firstChild->next;
        free(firstChild);
        firstChild=nextChild;
    }
}

void signalInitSegFault()
{
    struct sigaction sigIntHandler;
    sigIntHandler.sa_handler = (void (*)(int)) segFaultHandler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGINT, &sigIntHandler, NULL);
    sigaction(SIGSEGV, &sigIntHandler, NULL);

}

void segFaultHandler(int signum, siginfo_t *info, void *ucontext)
{
    exit(EXIT_FAILURE);
}

void cntrlCHandler()
{
    exit(EXIT_FAILURE);
}

