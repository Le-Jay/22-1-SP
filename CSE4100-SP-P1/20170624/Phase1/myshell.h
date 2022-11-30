#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#include <signal.h>
#include <dirent.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* External variables */
extern int h_errno;    /* Defined by BIND for DNS errors */
extern char **environ; /* Defined by libc */

/* Misc constants */
#define MAXLINE 8192
#define MAXBUF 8192
#define LISTENQ 1024

/* Standard I/O wrappers */
char *Fgets(char *ptr, int n, FILE *stream);