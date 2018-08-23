#include "unitTests.h"

/**
 * Hago un chequeo del archivo formado. Si hay mas de uno, agarra el primero que encontra
 */
int main(int argc, char* argv[])
{

    char * fileName;
    int ok;
    fileName = hasFile();
    ok = checkMD5(fileName);

    /**
     * Confirmo con un assert que todo corrio bien
     */
    assert(ok);

    printf("\nTermino. El proceso funciona\n");
    return 0;
}

/**
 * Chequeo en si esta el archivo creado por aplication.o en el directorio
 */
char * hasFile()
{
    int found = 1;
    DIR *dp;
    struct dirent *entry;
    struct stat statbuf;
    char * rt = NULL;

    if((dp = opendir(".")) == NULL)
    {
        fprintf(stderr,"cannot open directory: %s\n", ".");
        return NULL;
    }
    while(found && (entry = readdir(dp)) != NULL)
    {
        lstat(entry->d_name,&statbuf);
        if(S_ISDIR(statbuf.st_mode))
        {
            /* Found a directory, but ignore . and .. */
            if(strcmp(".",entry->d_name) == 0 || strcmp("..",entry->d_name) == 0)
                continue;
        }
        else
        {
            if(isFile(entry->d_name))
            {
                found = 0;
                rt = entry->d_name;
            }
        }
    }
    closedir(dp);
    return rt;
}

/**
 * Comparo el md5 calculado por aplication.o y el md5 calculado por este programa
 */
int checkMD5(char * fileName)
{
    if (fileName == NULL)
    {
        return FALSE;
    }

    FILE * file;
    char * path = malloc(1000);
    char md5Calculated[33];
    char md5Real[33];

    file = fopen(fileName, "r");

    char c;
    int mode = PATH;
    int i = 0;
    c = (char) fgetc(file);
    do
    {
        switch(mode)
        {
            case PATH:
                if(c == ' ')
                {
                    path[i] = '\0';
                    mode = MD5;
                    i = 0;
                }
                else
                {
                    path[i++] = c;
                }
                break;
            case MD5:
                if(!isxdigit(c) || i >= 32)
                {
                    mode = PATH;
                    md5Calculated[i] = '\0';
                    calculateMD5(path, md5Real);
                    free(path);
                    path = malloc(1000);
                    for (i = 0; i < 32; i++)
                    {
                        if(md5Real[i] != md5Calculated[i])
                        {
                            free(path);
                            return FALSE;
                        }
                    }
                    i = 0;
                }
                else
                {
                    md5Calculated[i++]  = c;
                }
                break;
            default:
                break;
        }
    }while((c = (char) fgetc(file)) != EOF);
    fclose(file);
    return TRUE;
}

/**
 * retorno 1 si encontre el archivo calculado por aplication.o es el @param
 */
int isFile(char * fileName)
{
    int i;
    char toCompare[] = "md5hash_";
    int size = (int) strlen(toCompare);
    for (i = 0; i < size && fileName[i] != '\0'; i++)
    {
        if(toCompare[i] != fileName[i])
        {
            return FALSE;
        }
    }
    return TRUE;
}

/**
 * Calculo el MD5 del path especificado
 */
void calculateMD5(char path[], char md5[32])
{
    FILE * file;
    char * md5SumFunc;
    int i;
    char ch;

    md5SumFunc = malloc((size_t) (strlen(path) + 8));
    strcat(md5SumFunc, "md5sum ");
    strcat(md5SumFunc, path);

    file = popen(md5SumFunc, "r");

    for (i = 0; i < 32 && isxdigit(ch = (char) fgetc(file)); i++)
    {
        md5[i] = ch;
    }
    md5[i] = '\0';

    free(md5SumFunc);
    pclose(file);
}
