#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef TEST
#include "tests.c"
#endif
#define MAXBUFSIZE 4096
#define MAX_ARGS 1024

#define exprealloc(ptr, size, type)                                                                          \
    do {                                                                                                     \
        size = 2 * size;                                                                                     \
        void *new = realloc(ptr, size * sizeof(type));                                                       \
        if (new == NULL) {                                                                                   \
            free(ptr);                                                                                       \
            return -1;                                                                                       \
        }                                                                                                    \
        ptr = new;                                                                                           \
    } while (0)

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
int get_words(char *restrict str, int *restrict wcp, char **restrict words, int maxw) {
#define IS_SPACE_CHAR(c) (c == ' ' || c == '\t' || c == '\n')
    int wc = 0;
    if (str == NULL || wcp == NULL || words == NULL)
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
    *wcp = wc;
    return 0;
}

int main(int argc, char *argv[]) {
#ifdef TEST
    return run_tests();
#endif

    char *line = malloc(512 * sizeof(char));
    int wcount;
    char *words[MAX_ARGS];
    size_t n;
    while (1) {
        puts("adash>");
        getline(&line, &n, stdin);
        if (-1 == get_words(line, &wcount, words, MAX_ARGS)) {
            printf("Cannot parse command\n");
            continue;
        }
    }
}
