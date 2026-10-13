#include <assert.h>
#include <pwd.h>
#include <stdint.h>
#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef TEST
#include "tests.c"
#endif
#define MAX_ARGS 1024
extern char **environ;

/*
 * @brief Print current working directory
 * @return 0 on success.
 * @return -1 on error. You can read errno.
 */
int print_cwd() {
    char *wd;
    wd = getcwd(NULL, 0);
    if (wd == NULL)
        return -1;
    if (fputs(wd, stdout) == EOF)
        return -1;
    free(wd);
    return 0;
}

/*
 * @brief Print current user (not effective user)
 * @warning if the user change after the first call, the user will not be updated.
 * @return 0 on success.
 * @return -1 on error. You can read errno.
 */
int print_user() {
    struct passwd *passwd = NULL;
    static char name[32] = {0};
    if (*name == '\0') {
        uid_t uid = getuid();
        passwd = getpwuid(uid);
        strcpy(name, passwd->pw_name);
    }
    if (fputs(name, stdout) == EOF)
        return -1;
    return 0;
}

/*
 * @brief print the query for the user
 * @return 0 on success
 * @return -1 on error with errno setted
 */
int print_query() {
    if (print_user() == -1)
        return -1;
    if (putchar(':') == -1)
        return -1;
    if (print_cwd() == -1)
        return -1;
    if (fputs("> ", stdout) == EOF)
        return -1;
    return 0;
}
/*
 * @brief Divide the string 'str' string using ' ' as separator.
 *
 * Scans the string and return a null terminated array of words that point to the same
 * memory location of the input string. '\0' is inserted instead of ' '.
 * The words are trimmed.
 *
 * @param[in,out] str           Null terminated string to parse. Must be a mutable string.
 * @param[out]  wcp             Words inside the string
 * @param[out]  words           A pointer to an array of strings pre allocated.
 * @param[in]   maxw            Size of the words array
 *
 * @return 0 on success
 * @return -1 on error
 *
 * @warning 'str' is modified in-place.
 */
int get_words(char *_Nonnull restrict str, int *_Nullable restrict wcp, char **_Nonnull restrict words,
              int maxw) {
#define IS_SPACE_CHAR(c) (c == ' ' || c == '\t' || c == '\n')
    int wc = 0;
    if (str == NULL || words == NULL)
        return -1;

    while (*str != '\0') {
        while (IS_SPACE_CHAR(*str) && *str != '\0')
            *str++ = '\0';

        if (*str == '\0')
            break;

        words[wc++] = str;
        if (wc >= maxw) {
            fprintf(stderr, "Too much arguments.\n");
            return -1;
        }
        while (!IS_SPACE_CHAR(*str) && *str != '\0')
            str++;
    }
    words[wc] = NULL;
    if (wcp != NULL)
        *wcp = wc;
    return 0;
}
/*
 * @brief create a new process
 */
int init_process(char *_Nonnull args[], int stop) {
    assert(args != NULL);
    if (args[0] == NULL)
        return 0;
    int pid = fork();
    if (pid == -1)
        return -1;
    if (pid != 0)
        return pid;

    signal(SIGTTOU, SIG_DFL);
    signal(SIGTTIN, SIG_DFL);
    signal(SIGINT, SIG_DFL);
    if (stop)
        raise(SIGSTOP);

    execvp(args[0], args);
    perror("");
    exit(1);
}

/*
 * must be called after init_process by the father
 */
int process_to_fg(int pid) {
    assert(pid != -1);
    assert(pid != 0);
    if (setpgid(pid, pid) == -1)
        return -1;
    if (tcsetpgrp(STDIN_FILENO, pid) == -1)
        return -1;
    return pid;
}

/*
 * I use this design instead of the suggested design from POSIX/GNU
 * developers of repeated calls in the father and in the children
 * of setpgid and tcsetpgrp. The overhead from the raise(SIGSTOP) and
 * kill is trascurable compared to the execution time of fork and execve.
 * With this design the code is more organized and the functions can be reused
 * also for the creation of background processes, so reducing the space for
 * errors
 */
int run_process_fg(char *_Nonnull argv[]) {
    assert(argv != NULL);
    if (argv[0] == NULL)
        return 0;
    int pid = init_process(argv, 1);
    int wstatus;
    if (pid == -1)
        return -1;
    if (process_to_fg(pid) == -1)
        return -1;
    if (-1 == waitpid(pid, &wstatus, WUNTRACED))
        return -1;
    if (WIFSTOPPED(wstatus))
        if (-1 == kill(pid, SIGCONT))
            return -1;
    if (waitpid(pid, NULL, 0) == -1)
        return -1;
    return pid;
}

/*
 * @brief check if argv contains a shell command and execute it
 * @return 1 if the execution succeed
 * @return 0 if the command is not a shell command
 * @return -1 if there is an error with the execution or the arguments.
 */
int exec_shell_cmd(char *_Nonnull argv[]) {
    assert(argv != NULL);
    if (argv[0] == NULL)
        return 0;
    if (strcmp(argv[0], "exit") == 0)
        exit(1);
    return 0;
}

int main(int argc, char *argv[]) {
#ifdef TEST
    return run_tests();
#endif
    char *line = malloc(512 * sizeof(char));
    char *args[MAX_ARGS];
    size_t n = 512;
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGINT, SIG_IGN);
    while (1) {
        if (-1 == print_query()) {
            perror("Cannot print query");
            exit(1);
        }
        getline(&line, &n, stdin);
        if (-1 == get_words(line, NULL, args, MAX_ARGS)) {
            printf("Cannot parse command\n");
            continue;
        }
        if (exec_shell_cmd(args) != 0)
            continue;
        if (-1 == run_process_fg(args)) {
            perror("");
            continue;
        }
        if (-1 == tcsetpgrp(STDIN_FILENO, getpgrp())) {
            perror("");
            exit(1);
        }
    }
    free(line);
    return 0;
}
