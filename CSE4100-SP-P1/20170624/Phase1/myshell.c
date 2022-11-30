#include "myshell.h"
#include <errno.h>
#define MAXARGS 128

void eval(char *cmdline);
int parseline(char *buf, char **argv);
int builtin_command(char **argv);
void unix_error(char *msg);
char root[MAXLINE]; // for adding /bin/
void interrupt_handler(int sig);

int main()
{
    char cmdline[MAXLINE];
    strcpy(root, "/bin/");

    do {
        signal(SIGTSTP, interrupt_handler);
        signal(SIGINT, interrupt_handler);
        printf("CSE4100:P1-myshell >");
        fgets(cmdline, MAXLINE, stdin);
        if(feof(stdin))
            exit(0);
        
        eval(cmdline);
    } while(1);

}

void eval(char *cmdline)
{
    char *argv[MAXARGS]; /* Argument list execve() */
    char buf[MAXLINE];   /* Holds modified command line */
    int bg;              /* Should the job run in bg or fg? */
    pid_t pid;           /* Process id */

    strcpy(buf, cmdline);
    bg = parseline(buf, argv);
    if (argv[0] == NULL)
        return;   /* Ignore empty lines */
    if (!builtin_command(argv)) { //quit -> exit(0), & -> ignore, other -> run
        pid = fork();
        if(pid < 0)
            unix_error("Fork error");
        else if(pid == 0) { //child process
            signal(SIGINT, SIG_DFL);
            signal(SIGTSTP, SIG_IGN);
            strcat(root, argv[0]); // ex) /bin/ls 에서 ls 붙
 
        
            if(execve(root,argv,environ) < 0) { //execve로 /bin/ls에서 명령어 못찾으면 -1 반환
                if(execve(argv[0], argv, environ) < 0) {       //ex) /bin/ls ls -al &
                    printf("%s: Command not found.\n", argv[0]);
                    exit(0);
                }
            }
        }
        /* Parent waits for foreground job to terminate */
        if (!bg){
            int status;
            if(waitpid(pid, &status, 0) < 0)
                unix_error("waitfg: waitpd error");
        }
        else//when there is backgrount process!
            printf("%d %s\n", pid, cmdline);
    }
    return;
}

int builtin_command(char **argv)
{
    if(!strcmp(argv[0], "exit") || !strcmp(argv[0], "quit"))
        exit(0);
    if(!strcmp(argv[0], "&"))    /* Ignore singleton & */
        return 1;
    if(!strcmp(argv[0], "cd")) {
        if(argv[1] == NULL)
            printf("Command : cd + directory\n");
        if(chdir(argv[1]) < 0){
            printf("%s: no such file or directory: %s\n", argv[0], argv[1]);
        }
        return 1;
    }

    return 0;                     /* Not a builtin command */
}

int parseline(char *buf, char **argv)
{
    char *delim;         /* Points to first space delimiter */
    int argc;            /* Number of args */
    int bg;              /* Background job? */

    buf[strlen(buf)-1] = ' ';  /* Replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* Ignore leading spaces */
        buf++;

    /* Build the argv list */
    argc = 0;
    while ((delim = strchr(buf, ' '))) {
        argv[argc++] = buf;
        *delim = '\0';
        buf = delim + 1;
        while (*buf && (*buf == ' ')) /* Ignore spaces */
            buf++;
    }
    argv[argc] = NULL;

    if (argc == 0)  /* Ignore blank line */
        return 1;

    /* Should the job run in the background? */
    if ((bg = (*argv[argc-1] == '&')) != 0)
        argv[--argc] = NULL;

    return bg;
}
/* $end parseline */


/* $begin errorfun */
void unix_error(char *msg) /* Unix-style error */
{
    fprintf(stderr, "%s: %s\n", msg, strerror(errno));
    exit(0);
}
/* $end errorfun */

void interrupt_handler(int sig)
{
    printf("\nCSE4100-SP-P1> \n");
}