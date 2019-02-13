//
// Created by mqt on 11/3/18.
//

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>
#include <time.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>



void ms_sighandler(int signo){
    if (signo == SIGINT) {}
    if (signo == SIGCHLD) {}
}

void execProc(char* _path, char** args, int isBG){
    // First try to find file in $PATH, then the current/provided dir _path
    pid_t child = fork();
    if (!child) {
        signal(SIGINT, SIG_DFL);
        if (execvp(args[0], args)) {
            if (execv(_path, args)) {
                printf("** Unable to execute ** : %s\n", strerror(errno));
                printf("      Pathname used: \"%s\"\n", _path);
                exit(errno);
            }
        }

        exit(0);
    }

    else {
        if (!isBG) {
            wait(0);
        } else {
            signal(SIGCHLD, SIG_IGN);
        }
    }
}

char *strtok_mcd(char *input, const char *delimiter) { //mcd: multi-char delimiter
    static char* rest[128];
    if (input != NULL)
        *rest = input;

    if (*rest == NULL)
        return *rest;

    char *end = strstr(*rest, delimiter);
    if (end == NULL) {
        char *temp = *rest;
        *rest = NULL;
        return temp;
    }

    char *temp = *rest;

    *end = '\0';
    *rest = end + strlen(delimiter);
    return temp;
}

char** tokenize(char* cmd, const char *delim, char* (*strtok_f)(char*, const char*)) {
    char** token_l = (char**)malloc(80);
    char* ostring = (char*)malloc(128);
    strcpy(ostring, cmd);
    int cnt = 0;
    char* raw_token = strtok_f(ostring, delim);
    while (raw_token) {
        while (raw_token[0] == ' ') { //eat space
            raw_token++;
        }
        token_l[cnt++] = raw_token;
        raw_token = strtok_f(NULL, delim);
    }
    return token_l;
}

void ms_pipe(char**);

int redirect(char* cmd, char *pathname, int mode, int flag){
    int f, dir_fd, dir_fd2, ofd, ofd2;
    char **tokens, **arg;

    if (mode > 2) {
        dir_fd = 1;
        dir_fd2 = 2;
    }
    else dir_fd = mode;

    if ((f = open(pathname, flag, S_IRWXU))<0) {
        printf("** Invalid pathname for redirection ** : %s\n", strerror(errno));
        printf("      Pathname used: \"%s\"\n", pathname);

        return errno;
    }
    ofd = dup(dir_fd);
    dup2(f, dir_fd);
    if (mode  > 2) {
        ofd2 = dup(dir_fd2);
        dup2(f, dir_fd2);
    }
    tokens = tokenize(cmd, " ", &strtok);
    execProc(tokens[0], tokens,0);
    fflush(stdout);
    fflush(stdin);
    fflush(stderr);
    close(f);
    dup2(ofd, dir_fd);
    if (mode  > 2) dup2(ofd2, dir_fd2);
    close(ofd);

}

void cmdParser(char* string) {
    char **arg;
    char **cmd = tokenize(string, ";", &strtok);
    int i = -1;
    while (cmd[++i]){
        if ((arg = tokenize(cmd[i], "|", &strtok))[1]){
            ms_pipe(arg);
            break;
        }
        else if ((arg = tokenize(cmd[i], ">", &strtok))[1]){
            if ((arg = tokenize(cmd[i], "1>", &strtok_mcd))[1])
                redirect(arg[0], arg[1],1, O_CREAT|O_TRUNC|O_WRONLY);
            else if ((arg = tokenize(cmd[i], "2>", &strtok_mcd))[1])
                redirect(arg[0], arg[1],2, O_CREAT|O_TRUNC|O_WRONLY);
            else if ((arg = tokenize(cmd[i], "&>", &strtok_mcd))[1])
                redirect(arg[0], arg[1],3, O_CREAT|O_TRUNC|O_WRONLY);
            else {
                arg = tokenize(cmd[i], ">", &strtok);
                redirect(arg[0], arg[1],1, O_CREAT|O_TRUNC|O_WRONLY);
            }
        } else if ((arg = tokenize(cmd[i], "<", &strtok))[1]) {
            redirect(arg[0], arg[1], 0, O_RDONLY);
        } else if (cmd[i][strlen(cmd[i]) - 1] == '&') {
            cmd[i][strlen(cmd[i]) - 1] = '\000';
            arg = tokenize(cmd[i], " ", &strtok);
            execProc(arg[0], arg,1);
        } else {
            arg = tokenize(cmd[i], " ", &strtok);
            execProc(arg[0], arg,0);
        }
    }
}

void ms_pipe_helper(int in, int out, char* cmds) {
    if (fork() == 0) {
        if (in != 0) {
            dup2(in, 0);
            close(in);
        }
        if (out != 1){
            dup2(out, 1);
            close(out);
        }
        cmdParser(cmds);
        exit(0);
    } else {
        wait(0);
    }

}
void ms_pipe(char** cmds){
    int pfd[2], ostdin, nextInput;
    int i = 0;
    nextInput = 0; //first read from stdin
    ostdin = dup(0); //back up original copy of stdin

//
    do {
        pipe(pfd);
        ms_pipe_helper(nextInput, pfd[1], cmds[i]);
        close(pfd[1]); //no need the write end any more
        nextInput = pfd[0]; //next child read from prev's "stdout"

    }while (cmds[++i]&&cmds[i+1]);
    //last stage: in is from prev out, and out is to stdout
    if (nextInput != 0) {
        dup2(nextInput, 0);
    }
    cmdParser(cmds[i]);
    fflush(stdin);
    dup2(ostdin, 0); // restore old stdin
    close(ostdin);
}



int main() {
    char *string;
    signal(SIGINT, ms_sighandler);
    while (1){
        string = (char*)malloc(80);
        printf("myshell> ");
        if (!fgets(string, 80, stdin)){
            printf("\nExiting myshell...\n");
            exit(0);
        }
        if (*string == '\n') {
            continue;
        }
        string[strlen(string) - 1] = '\000'; //new-line eating
        cmdParser(string);
        free(string);
    }
}
