#include "myshell.h"
#include <errno.h>
#define MAXARGS 128

void eval(char *cmdline);
void eval_p(char *cmdline);
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
        if(!strstr(cmdline, "|"))
            eval(cmdline);
        else eval_p(cmdline); //single pipe + multi pipe
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
                if((argv[0], argv, environ) < 0) {       //ex) /bin/ls ls -al &
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

void eval_p(char *cmdline)
{
    char *argv[MAXARGS];
    char buf[MAXLINE];
    int bg;
    pid_t pid;

    //file descriptor
    int fd1[2];
    int fd2[2];
    char* p_cmd[MAXARGS]; //pipe로 구분된 commands

    strcpy(buf, cmdline);
    bg = parseline(buf, argv);
    if(argv[0] == NULL) return; //공백 무시
    
    int i = 0;
    int j = 0;
    while(1) {
        for( ; i < MAXARGS ; i++) {
            if(argv[i] == NULL) {
                p_cmd[j] = NULL;
                j = 0;
                break;
            }
            // "|"를 찾은 경우
            else if(strcmp("|", argv[i]) == 0) {
                p_cmd[j] = NULL;
                j = 0;
                i++;
                break;
            }
            else {
                p_cmd[j] = (char*)malloc(sizeof(char) * MAXLINE);
                strcpy(p_cmd[j], argv[i]);
                j++;
            }
        }
        int pipe_val;
        pipe_val = pipe(fd1); //create pipe
        if(pipe_val < 0)
            unix_error("pipe error\n");
        if(!builtin_command(p_cmd)) {
            pid = fork();
            if(pid < 0)
                unix_error("Fork error\n");

            else if(pid == 0) { //child process
                //ctrl+C interrupt
                signal(SIGINT, SIG_DFL);
                signal(SIGTSTP, SIG_IGN);
                
                //redirection. fd2 => 이전의 pipe
                dup2(fd2[0], STDIN_FILENO);
                if(argv[i] != NULL) //아직 다 안썼으면 fd1에 써야..
                    dup2(fd1[1], STDOUT_FILENO);
                close(fd1[0]);

                strcat(root, p_cmd[0]);
                
                if(execve(root, p_cmd, environ) < 0) {
                        printf("%s: Command not found.\n", p_cmd[0]);
                        exit(0);
                }
                
            if(argv[i] == NULL) break;
            }
            else {//parent process
                if(!bg) {
                    int status;
                    if(waitpid(pid, &status, 0) < 0) //parent wait
                        unix_error("waitfg: waitpd error");
                    close(fd1[1]);
                    fd2[0] = fd1[0];
                    fd2[1] = fd1[1]; //for multiple pipeline.
                }
                else//background process가 있을때
                    printf("%d %s\n", pid, cmdline);
            }
        }
        if(argv[i] == NULL) break;
    }
    return;
}
/*end of eval_p*/

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