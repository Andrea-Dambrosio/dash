#include <assert.h>
#include <errno.h>
#include <linux/limits.h>
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

const int SIGNALS[] = {SIGINT, SIGTTIN, SIGTTOU};

// Utils functions

/*
 * @brief Disable for the current process the signals defined in SIGNALS
 * @return 0 on success
 * @return -1 on error and set errno
 */
static inline int disable_signals() {
    int count = sizeof(SIGNALS) / sizeof(SIGNALS[0]);
    for (int i = 0; i < count; i++)
        if (signal(SIGNALS[i], SIG_IGN) == SIG_ERR)
            return -1;
    return 0;
}
/*
 * @brief Enable for the current process the signals defined in SIGNALS
 * @return 0 on success
 * @return -1 on error and set errno
 */
static inline int enable_signals() {
    int count = sizeof(SIGNALS) / sizeof(SIGNALS[0]);
    for (int i = 0; i < count; i++)
        if (signal(SIGNALS[i], SIG_DFL) == SIG_ERR)
            return -1;
    return 0;
}

/*
 * @brief Return current working directory
 * @return A string pointer to the PWD env variable value on success.
 * @return NULL on error, you can read errno
 */
char *PWD = NULL;
char *getpwd() {
    if (PWD != NULL)
        return PWD;
    PWD = getenv("PWD");
    if (PWD == NULL) {
        PWD = getcwd(NULL, 0);
        if (PWD == NULL)
            return NULL;
        setenv("PWD", PWD, 1);
        free(PWD);
    }
    PWD = getenv("PWD");
    return PWD;
}

/*
 * @brief Set pwd
 * @return 0 on succes
 * @return -1 on error. Sets errno.
 */
int setpwd(char *path) {
    char *curr = getpwd();
    if (chdir(path) == -1)
        return -1;
    if (setenv("PWD", path, 1) == -1)
        return -1;
    if (curr != NULL)
        if (setenv("OLDPWD", curr, 1) == -1)
            return -1;
    PWD = getenv("PWD");
    assert(PWD != NULL);
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
    if (fputs(getpwd(), stdout) == EOF)
        return -1;
    if (fputs("> ", stdout) == EOF)
        return -1;
    return 0;
}
/*
 * @brief Split the string 'str' string using 'sep' as separator.
 * Supports the escape char and '"'
 *
 * Scans the string and return a null terminated array of words that point to the same
 * memory location of the input string. '\0' is inserted instead of ' '.
 * All escape chars are removed, a char after \ is not evaluated.
 * The a string inside the " " is copied without the '"'. If the closing " is missing everything is copied
 * until the end. The words are trimmed.
 *
 * @param[in,out] str           Null terminated string to parse. Must be a mutable string.
 * @param[out]  wcp             Words inside the string
 * @param[out]  words           A pointer to an array of strings pre allocated.
 * @param[in]   maxw            Size of the words array
 * @param[in]   sep             Separator of the words
 *
 * @return 0 on success
 * @return -1 on error. Sets errno.
 *
 * @warning 'str' is modified in-place.
 */

int splitstr(char *_Nonnull restrict str, int *_Nullable restrict wcp, char **_Nonnull restrict words,
             int maxw, char sep) {
#define IS_SEP_CHAR(c, s) ((c) == '\t' || (c) == '\n' || (c) == (s))
    int wc = 0;
    char *r, *w;
    if (str == NULL || words == NULL || maxw <= 0) {
        errno = EINVAL;
        return -1;
    }
    r = str;
    w = str;
    while (*r != '\0') {
        while (*r != '\0' && IS_SEP_CHAR(*r, sep))
            r++;

        if (*r == '\0')
            break;

        words[wc++] = w;
        if (wc >= maxw) {
            errno = E2BIG;
            return -1;
        }
        while (*r != '\0' && !IS_SEP_CHAR(*r, sep)) {
            if (*r == '\\') {
                r++;
                if (*r == '\0')
                    break;
            } else if (*r == '"') {
                r++;
                while (*r != '\0' && *r != '\n' && *r != '"')
                    *w++ = *r++;
                if (*r == '"')
                    r++;
                continue;
            }

            *w++ = *r++;
        }
        if (*r != '\0')
            r++;
        *w++ = '\0';
    }
    words[wc] = NULL;
    if (wcp != NULL)
        *wcp = wc;
    return 0;
}
/*
 * @brief create a new process
 * Create a new process using fork and execvp. Enable all the signals.
 * @param args[in] array of strings with the program and its arguments. The first string should be the
 * program name.
 * @param stop[in] if true send SIGSTOP just before execvp
 * @param pgid[in] Set the pgid. If 0 the pgid will be the pid of the new process. If -1 pgid will not be
 * edited.
 *
 * @return -1 on error, sets errno
 * @return pid of the new process to the father
 * @noreturn for the child.
 */
int init_process(char *_Nonnull args[], int stop, int pgid) {
    if (pgid < -1) {
        errno = EINVAL;
        return -1;
    }
    if (args[0] == NULL)
        return 0;

    int pid = fork();
    if (pid == -1)
        return -1;
    if (pid != 0)
        return pid;

    if (-1 == enable_signals()) {
        perror("Child cannot enable signals");
        exit(1);
    }
    if (pgid != -1)
        if (setpgid(0, pgid) == -1) {
            perror("Child cannot change pgid");
            exit(1);
        }

    if (stop)
        raise(SIGSTOP);

    execvp(args[0], args);
    perror("");
    exit(1);
}

/*
 * @brief Set the TTY (described by STDIN) foregroud group to pgid
 * @return 0 on success
 * @return 1 on error
 */
int connect_TTY(int pgid) {
    assert(pgid >= 0);
    if (tcsetpgrp(STDIN_FILENO, pgid) == -1)
        return -1;
    return 0;
}

/*
 * @brief Wait for SIGSTOP of a process and send SIGCONT after.
 * @note if waitpid return a status different from WIFSTOPPED the procedure
 * will return succes without sending any signal.
 *
 * @param pid Process id to wait
 * @return 0 on success
 * @return -1 on error. Sets errno.
 */
int wawakep(int pid) {
    int wstatus;
    if (-1 == waitpid(pid, &wstatus, WUNTRACED))
        return -1;
    if (WIFSTOPPED(wstatus))
        if (-1 == kill(pid, SIGCONT))
            return -1;
    return 0;
}

/*
 * @brief Run the program defined in argv[0] with args argv[1:...] in foreground. Wait for
 * the program to exit.
 *
 * @note I use this design instead of the suggested design from POSIX/GNU
 * of repeated calls in the father and in the children
 * of setpgid and tcsetpgrp. The overhead from the raise(SIGSTOP) and
 * kill is trascurable compared to the execution time of fork and execve.
 * With this design the code is more organized and the functions can be reused
 * also for the creation of background processes, so reducing the space for
 * errors
 *
 * @return exit code of the executed program
 * @return -1 on error
 *
 * @warning If argv[0] == NULL no program will be executed and 0 will be returned
 */
int rprocfg_wait(char *_Nonnull argv[]) {
    int wstatus, pid;
    assert(argv != NULL);
    if (argv[0] == NULL)
        return 0;
    pid = init_process(argv, 1, 0);
    if (pid == -1)
        return -1;
    if (connect_TTY(pid) == -1)
        return -1;
    if (wawakep(pid) == -1)
        return -1;
    if (waitpid(pid, &wstatus, 0) == -1)
        return -1;
    return WEXITSTATUS(wstatus);
}

/*
 * @brief Parse a path, create a clean path without '..', '.' and ''
 * @return 0 on success
 * @return -1 on error (only if the string contains more than STACK_SIZE tokens). Sets errno
 * @warning The escape char and the '"' char are not managed here
 */
int clean_path(char *path) {
    const int STACK_SIZE = 256;
    char *stack[STACK_SIZE];
    char tmp[strlen(path) + 1];
    int top = 0;

    char *tok = strtok(path, "/");
    while (tok != NULL) {
        if (strcmp(tok, ".") == 0)
            goto next;
        if (strcmp(tok, "..") == 0) {
            top = top > 0 ? top - 1 : 0;
            goto next;
        }
        if (top > STACK_SIZE) {
            errno = ENAMETOOLONG;
            return -1;
        }
        stack[top++] = tok;
    next:
        tok = strtok(NULL, "/");
    }

    tmp[0] = '\0';
    for (int i = 0; i < top; i++) {
        strcat(tmp, "/");
        strcat(tmp, stack[i]);
    }
    strcpy(path, tmp);
    return 0;
}

/*
 * @brief Implements the cd command.
 * Change the current logical directory to the user interface.
 * Resolve the logical path and sets the physical directory for the process.
 * @param[in] path The relative (or absolute) path to go.
 * @return 0 on succes
 * @return -1 on error. Sets errno
 * @warning This function uses PATH_MAX, it may cause problem in some use cases.
 */
int cd(char *path) {
    char *currdir, buff[PATH_MAX + 1];
    char *target;
    currdir = getpwd();
    if (currdir == NULL)
        return -1;
    if (path == NULL || *path == '\0') {
        target = getenv("HOME");
        target = target == NULL ? "/" : target;
        goto chdir;
    }
    if (strcmp(path, "-") == 0) {
        target = getenv("OLDPWD");
        target = target != NULL ? target : currdir;
        goto chdir;
    }
    if (path[0] == '/') {
        target = path;
        if (-1 == clean_path(target))
            return -1;
        goto chdir;
    }
    snprintf(buff, PATH_MAX + 1, "%s/%s", currdir == NULL ? "" : currdir, path);
    target = buff;
    if (-1 == clean_path(target))
        return -1;

chdir:
    return setpwd(target);
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
    if (strcmp(argv[0], "cd") == 0) {
        if (argv[2] != NULL)
            fprintf(stderr, "Too much arguments\n");
        if (cd(argv[1]) == -1) {
            perror("cd");
            return -1;
        }
        return 1;
    }
    return 0;
}

int main(int argc, char *argv[]) {
#ifdef TEST
    return run_tests();
#endif
    char *line = malloc(512 * sizeof(char));
    char *args[MAX_ARGS];
    size_t n = 512;
    if (-1 == disable_signals()) {
        perror("Cannot disable signals");
        exit(1);
    }
    while (1) {
        if (-1 == print_query()) {
            perror("Cannot print query");
            exit(1);
        }
        getline(&line, &n, stdin);
        if (-1 == splitstr(line, NULL, args, MAX_ARGS, ' ')) {
            printf("Cannot parse command\n");
            continue;
        }
        if (exec_shell_cmd(args) != 0)
            continue;
        if (-1 == rprocfg_wait(args)) {
            perror("Error while executing new process");
            continue;
        }
        if (connect_TTY(getpgrp()) == -1) {
            perror("Dash cannot reconnect to TTY");
            exit(1);
        }
    }
    free(line);
    return 0;
}
