#include "slaveProtocol.h"

int main(int argc, char* argv[])
{
    char amountofPaths;
    pid_t parent = getppid();
    int i;
    int j;
    int ch;
    FILE * file;
    char * md5SumFunc;


    while(kill(parent, 0) == 0)
    {
        char buf[BUFFER_SIZE];

        if(read(STDIN_FILENO, buf, BUFFER_SIZE) >= 0)
        {
            amountofPaths = buf[0];
            j = 1;

            while(amountofPaths > 0)
            {
                char pathNameHash[BUFFER_SIZE];
                char errorMessage[BUFFER_SIZE] = " Error: ";
                errorMessage[0] = (char) 255;
                int size = getPath(pathNameHash, buf, j);
                pathNameHash[0] = (char) (amountofPaths - 1);
                j += size;
                md5SumFunc = malloc((size_t) (size + 8));
                strcat(md5SumFunc, "md5sum ");
                strcat(md5SumFunc, &pathNameHash[1]);
                strcat(errorMessage, &pathNameHash[1]);
                strcat(errorMessage, " does not exist\0");

                file = popen(md5SumFunc, "r");

                pathNameHash[size] = ' ';
                for (i = 1; i < HASH_SIZE && isxdigit(ch = fgetc(file)); i++)
                {
                    pathNameHash[size+i] = (char) ch;
                }
                pathNameHash[size+i] = '\0';

                free(md5SumFunc);
                pclose(file);
                if(checkPathName(pathNameHash))
                {
                    write(STDOUT_FILENO, errorMessage, BUFFER_SIZE);
                }
                else
                {
                    write(STDOUT_FILENO, pathNameHash, BUFFER_SIZE);
                }
                amountofPaths--;
            }
        }
        else
        {
            perror("Error reading from pipe slave\n");
        }
    }
    exit(EXIT_SUCCESS);
}

int checkPathName(char pathNameHash[BUFFER_SIZE])
{
    int i = 0;

    while(pathNameHash[i] != ' ')
    {
        i++;
    }

    for (; pathNameHash[i]!='\0'; i++)
    {
        if (!isxdigit(pathNameHash[i]))
        {
            return 0;
        }
    }

    return 1;
}

/** agarro del buffer el path y retorno el tamano con el 0 del path */
int getPath(char pathName[BUFFER_SIZE], char buf[BUFFER_SIZE], int i)
{
    int ret = 0;

    while (buf[ret+i] != 0)
    {
        pathName[ret+1] = buf[ret+i];
        ret++;
    }
    pathName[ret+1] = buf[ret+i];

    return ++ret;
}
