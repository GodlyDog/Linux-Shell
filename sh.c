#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include "jobs.h"

int num_jobs = 0;
/*
 * Parses string command line input into an argument vector. Separates on
 * whitespace, and does not handle file path trimming (this is done later)
 */
int parse(char buffer[1024], char *tokens[512], char *argv[512]) {
    int num_tokens = 0;
    if (buffer[0] != '\0') {
        char *separator = " \n\t";
        char *token = strtok(buffer, separator);
        if (token != NULL) {
            tokens[0] = token;
            int i = 1;
            while (token != NULL) {
                token = strtok(NULL, separator);
                tokens[i] = token;
                i++;
            }
            int j = 0;
            while (tokens[j] != NULL) {
                argv[j] = tokens[j];
                j++;
            }
            num_tokens = j;
        }
    }
    return num_tokens;
}

/*
 * Handles execution of files passed to the command line. Uses the flags in and
 * out_type to determine whether file redirection should occur. Output and input
 * store the filenames to use in redirection. Forks from the main process,
 * closes stdin or stdout if file redirection should occur, then uses execv to
 * execute the desired process
 */
int exec_handler(int i, char *argv[512], int num_tokens, int out_type, int in,
                 char *output[512], char *input[512], void* jobList) {
    char *args[512];
    memset(args, 0, sizeof(args));
    /* handle parsing of first arg in argv to final path in first element of
     * args */
    if (strstr(argv[0], "/")) {
        /* filepath trimming */
        char *firstArg = strrchr(argv[0], '/');
        args[0] = &firstArg[1];
    }
    if (num_tokens >= 1) {
        for (int j = 1; j < num_tokens; j++) {
            args[j] = argv[j];
        }
        int background = 0;
        pid_t gid = getpgrp();
        if (args[0] != NULL) {
            if (strcmp(args[num_tokens - 1], "&") == 0) {
                background = 1;
                args[num_tokens - 1] = NULL;
                num_tokens--;
                num_jobs++;
            }
            int status = 0;
            pid_t pid = fork();
            if (pid < 0) {
                perror("fork error");
            } else {
                if (!pid) {
                    setpgid(pid, pid);
                    if (!background) {
                        pid_t gpid = getpgrp();
                        tcsetpgrp(0, gpid);
                    }
                    signal(SIGINT, SIG_DFL);
                    signal(SIGTSTP, SIG_DFL);
                    signal(SIGTTOU, SIG_DFL);
                    if (in == 1) {
                        close(fileno(stdin));
                        open(input[0], O_RDONLY, 0666);
                    }
                    if (out_type == 1) {
                        close(fileno(stdout));
                        open(output[0], O_WRONLY | O_CREAT | O_TRUNC, 0666);
                    } else if (out_type == 2) {
                        close(fileno(stdout));
                        open(output[0], O_WRONLY | O_CREAT | O_APPEND, 0666);
                    }
                    execv(argv[i], args);
                    sleep(1000);
                    remove_job_pid(jobList, getpid());
                    num_jobs--;
#ifdef PROMPT
                    printf("Execution error\n");
                    fflush(stdout);
#endif
                    tcsetpgrp(0, gid);
                    exit(status);
                } else {
                    if (background) {
                        add_job(jobList, num_jobs, pid, RUNNING, argv[i]);
                        int jid = get_job_jid(jobList, pid);
                        printf("%s%d%s%d%s", "[", jid, "] (", pid, ")\n");
                        fflush(stdout);
                    } else {
                        int status;
                        waitpid(pid, &status, WUNTRACED);
                        if (WIFSTOPPED(status)) {
                            num_jobs++;
                            add_job(jobList, num_jobs, pid, STOPPED, argv[i]);
                            printf("%s%d%s%d%s%d%s", "[", num_jobs, "] (", pid, ") suspended by signal ",
                                   WSTOPSIG(status),
                                   "\n");
                            fflush(stdout);
                            tcsetpgrp(0, gid);
                        }
                        if (WIFSIGNALED(status)) {
                            num_jobs++;
                            printf("%s%d%s%d%s%d%s", "[", num_jobs, "] (", pid, ") terminated by signal ",
                                   WTERMSIG(status),
                                   "\n");
                            fflush(stdout);
                        }
                        if (WIFCONTINUED(status)) {
                            update_job_pid(jobList, pid, RUNNING);
                        }
                    }
                    tcsetpgrp(0, gid);
                }
            }
        } else {
            return -1;
        }
    } else {
        return -1;
    }
    return 0;
}

/*
 * Reaping function. The function is executed at the beginning of the while loop in main, and handles any change in
 * the status of background processes.
 */
void grim(void* jobList) {
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        if (WIFSIGNALED(status)) {
            int jid = get_job_jid(jobList, pid);
            remove_job_jid(jobList, jid);
            num_jobs--;
            printf("%s%d%s%d%s%d%s", "[", jid, "] (", pid, ") terminated by signal ", WTERMSIG(status), "\n");
            fflush(stdout);
        } else if (WIFEXITED(status)) {
            int jid = get_job_jid(jobList, pid);
            remove_job_jid(jobList, jid);
            num_jobs--;
            printf("%s%d%s%d%s%d%s", "[", jid, "] (", pid, ") terminated with exit status ", WEXITSTATUS(status), "\n");
            fflush(stdout);
        } else if (WIFCONTINUED(status)) {
            update_job_pid(jobList, pid, RUNNING);
            int jid = get_job_jid(jobList, pid);
            printf("%s%d%s%d%s", "[", jid, "] (", pid, ") resumed\n");
        } else if (WIFSTOPPED(status)) {
            update_job_pid(jobList, pid, STOPPED);
            int jid = get_job_jid(jobList, pid);
            printf("%s%d%s%d%s%d%s", "[", jid, "] (", pid, ") suspended by signal ", WSTOPSIG(status), "\n");
        }
    }
}
/*
 * Handles resuming a process in the foreground. Sends signal to resume, then waits for updates with waitpid
 */
void fg(void* jobList, pid_t pid, int jid) {
        pid_t shellgid = getpgrp();
        pid_t gid = getpgid(pid);
        tcsetpgrp(0, gid);
        if (kill(-1 * gid, SIGCONT) < 0) {
            perror("Error while sending signal");
        }
        update_job_pid(jobList, pid, RUNNING);
        int status;
        waitpid(pid, &status, WUNTRACED);
        if (WIFSTOPPED(status)) {
            update_job_pid(jobList, pid, STOPPED);
            printf("%s%d%s%d%s%d%s", "[", jid, "] (", pid, ") suspended by signal ", WSTOPSIG(status), "\n");
            fflush(stdout);
            tcsetpgrp(0, shellgid);
        }
        if (WIFSIGNALED(status)) {
            remove_job_jid(jobList, jid);
            printf("%s%d%s%d%s%d%s", "[", jid, "] (", pid, ") terminated by signal ", WTERMSIG(status), "\n");
            fflush(stdout);
        }
        if (WIFCONTINUED(status)) {
            update_job_pid(jobList, pid, RUNNING);
        }
        if (WIFEXITED(status)) {
            remove_job_pid(jobList, pid);
        }
    tcsetpgrp(0, shellgid);
}

/*
 * Handles all basic commands like rm, ln, cd, and exit. Also sets flags and
 * files for file redirection. Otherwise, unknown commands are passed to
 * exec_handler. If exec_handler fails, output error message/ return to
 * beginning of input loop
 */
int main() {
    /* TODO: everything! */
#ifdef PROMPT
    char cwd[1024];
#endif
    char buf[1024];
    char *argv[512];
    char *tokens[512];
    char *input[512];
    char *output[512];
    int status = 0;
    if (signal(SIGINT, SIG_IGN) == SIG_ERR) {
        perror("Signal Error");
    }
    if (signal(SIGTSTP, SIG_IGN) == SIG_ERR) {
        perror("Signal Error");
    }
    if (signal(SIGTTOU, SIG_IGN) == SIG_ERR) {
        perror("Signal Error");
    }
    void* jobList = init_job_list();
start:
    while (1) {
#ifdef PROMPT
        if (printf("33 Shell: ") < 0) {
            write(2, "Prompt write error", sizeof("Prompt write error"));
        }
        if (fflush(stdout) < 0) {
            write(2, "Prompt flush error", sizeof("Prompt flush error"));
        }
#endif
        /* resetting memory for beginning of loop */
        memset(input, 0, sizeof(input));
        memset(output, 0, sizeof(output));
        memset(buf, 0, sizeof(buf));
        memset(argv, 0, sizeof(argv));
        memset(tokens, 0, sizeof(tokens));
        grim(jobList);
        ssize_t eof = read(0, buf, sizeof(buf));
        if (eof == 0) {
            cleanup_job_list(jobList);
            exit(1);
        }
        int out_type = 0;
        int in = 0;
        int num_tokens = parse(buf, tokens, argv);
        char *argcopy[512];
        if (argv[0] != NULL) {
            if (num_tokens >= 1) {
                for (int i = 0; i < num_tokens; i++) {
                    /*
                     * This section handles file redirection. Each if statement
                     * checks for a redirection, checks if the appropriate flag
                     * has already been set, sets the file for redirecting,
                     * removes the redirection symbol and redirection file from
                     * argv, then decrements the number of tokens by 2 and sets
                     * the corresponding redirection flag
                     */
                    if (!strcmp(argv[i], ">")) {
                        if (out_type != 0) {
                            fprintf(stderr,
                                    "syntax error: multiple output files");
                            goto start;
                        }
                        output[0] = argv[i + 1];
                        int offset = 0;
                        for (int j = 0; j < num_tokens;) {
                            if (strcmp(argv[j], output[0]) &&
                                strcmp(argv[j], ">")) {
                                argcopy[j - offset] = argv[j];
                                j++;
                            } else {
                                j++;
                                offset++;
                            }
                        }
                        i--;
                        memcpy(argv, argcopy, sizeof(argv));
                        num_tokens = num_tokens - 2;
                        out_type = 1;
                    } else if (!strcmp(argv[i], "<")) {
                        if (in == 1) {
                            fprintf(stderr,
                                    "syntax error: multiple input files");
                            goto start;
                        }
                        input[0] = argv[i + 1];
                        int offset = 0;
                        for (int j = 0; j < num_tokens;) {
                            if (strcmp(argv[j], input[0]) &&
                                strcmp(argv[j], "<")) {
                                argcopy[j - offset] = argv[j];
                                j++;
                            } else {
                                j++;
                                offset++;
                            }
                        }
                        i--;
                        memcpy(argv, argcopy, sizeof(argv));
                        num_tokens = num_tokens - 2;
                        in = 1;
                    } else if (!strcmp(argv[i], ">>")) {
                        if (out_type != 0) {
                            fprintf(stderr,
                                    "syntax error: multiple output files");
                            goto start;
                        }
                        output[0] = argv[i + 1];
                        int offset = 0;
                        for (int j = 0; j < num_tokens;) {
                            if (strcmp(argv[j], output[0]) &&
                                strcmp(argv[j], ">>")) {
                                argcopy[j - offset] = argv[j];
                                j++;
                            } else {
                                j++;
                                offset++;
                            }
                        }
                        i--;
                        memcpy(argv, argcopy, sizeof(argv));
                        num_tokens = num_tokens - 2;
                        out_type = 2;
                    }
                }
            }
            /* builtin commands are handled here */
            if (strcmp(argv[0], "cd") == 0) {
                if (1 < num_tokens) {
                    status = chdir(argv[1]);
                    if (status < 0) {
                        perror("chdir error");
                    }
                } else {
                    fprintf(stderr, "cd: syntax error\n");
                }
#ifdef PROMPT
                if (status < 0) {
                    printf("\nInvalid filename\n");
                    fflush(stdout);
                } else {
                    getcwd(cwd, sizeof(cwd));
                    printf("%s\n", cwd);
                    fflush(stdout);
                }
#endif
            } else if (strcmp(argv[0], "fg") == 0) {
                char* jidArg = argv[1];
                int jid = atoi(jidArg + 1);
                pid_t pid = get_job_pid(jobList, jid);
                if (pid > 0) {
                    fg(jobList, pid, jid);
                } else {
                    printf("job not found\n");
                    fflush(stdout);
                }
                goto start;
            } else if (strcmp(argv[0], "bg") == 0) {
                char* jidArg = argv[1];
                int jid = atoi(jidArg + 1);
                pid_t pid = get_job_pid(jobList, jid);
                if (pid > 0) {
                    kill(-1 * pid, SIGCONT);
                    update_job_pid(jobList, pid, RUNNING);
                } else {
                    printf("job not found\n");
                    fflush(stdout);
                }
                goto start;
            }
            else if (strcmp(argv[0], "jobs") == 0) {
                jobs(jobList);
            }
            else if (strcmp(argv[0], "rm") == 0) {
                    if (1 < num_tokens) {
                        status = unlink(argv[1]);
                    }
                    if (status < 0) {
                        perror("Invalid filename");
                    }
                }
            else if (strcmp(argv[0], "ln") == 0) {
                if (2 < num_tokens) {
                    status = link(argv[1], argv[2]);
                }
                if (status < 0) {
                    perror("Invalid filename");
                    fflush(stdout);
                }
            } else if (strcmp(argv[0], "exit") == 0) {
                if (1 < num_tokens) {
                    status = atoi(argv[1]);
                }
#ifdef PROMPT
                printf("Exiting with status: ");
                fflush(stdout);
                printf("%d", status);
                fflush(stdout);
                printf("\n");
                fflush(stdout);
#endif
                cleanup_job_list(jobList);
                exit(status);
            } else {
                status = exec_handler(0, argv, num_tokens, out_type, in, output,
                                      input, jobList);
#ifdef PROMPT
                if (status < 0) {
                    fprintf(stderr, "Unrecognized command/file or directory invalid: ");
                    fflush(stdout);
                    printf("%s", argv[0]);
                    fflush(stdout);
                    printf("\n");
                    fflush(stdout);
                    goto start;
                }
#endif
                goto start;
            }
        }
    }
}
