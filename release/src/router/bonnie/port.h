#ifndef PORT_H
#define PORT_H

#include <stdio.h>

#if defined (WIN32) || defined (OS2)
#define NON_UNIX
#endif


#define USE_SA_SIGACTION



#if 0
#define false 0
#define true 1
#endif

#ifdef OS2
#define NO_SNPRINTF
typedef enum
{
  false = 0,
  true = 1
} bool;

#define INCL_DOSQUEUES
#include <os2.h>

#define rmdir(XX) { DosDeleteDir(XX); }
#define chdir(XX) DosSetCurrentDir(XX)
#define file_close(XX) { DosClose(XX); }
#define make_directory(XX) DosCreateDir(XX, NULL)
typedef HFILE FILE_TYPE;
#define pipe(XX) DosCreatePipe(&XX[0], &XX[1], 8 * 1024)
#define sleep(XX) DosSleep((XX) * 1000)
#define exit(XX) DosExit(EXIT_THREAD, XX)
#else
#define file_close(XX) { ::close(XX); }
#define make_directory(XX) mkdir(XX, S_IRWXU)
typedef int FILE_TYPE;
#endif
typedef FILE_TYPE *PFILE_TYPE;
//typedef FILE *PFILE;

#endif

#ifdef NO_SNPRINTF
#define snprintf sprintf
#endif

#define EXIT_CTRL_C 5
