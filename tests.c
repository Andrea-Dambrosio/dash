#include <assert.h>
#include <stdio.h>
#include <string.h>

#define TEST_MAX_ARGS 64

// Function prototype from main.c
int get_words(char *restrict str, int *restrict wcp, char **restrict words, int maxw);

/**
 * @brief Unit tests for get_words function.
 *
 * Tests compliance with the function specification:
 * - Basic word parsing and trimming
 * - Multiple consecutive spaces, tabs, and newlines
 * - Leading and trailing whitespace
 * - Single-word commands
 * - Empty string and whitespace-only strings
 * - NULL-termination of the words array
 * - Error handling for NULL parameters and buffer limits
 */
int test_get_words(void) {
    char *words[TEST_MAX_ARGS];
    int wc = 0;
    int res = 0;

    printf("Running test_get_words...\n");

    // Test 1: Standard command with multiple arguments
    {
        char input[] = "echo hello world";
        res = get_words(input, &wc, words, TEST_MAX_ARGS);
        assert(res == 0);
        assert(wc == 3);
        assert(strcmp(words[0], "echo") == 0);
        assert(strcmp(words[1], "hello") == 0);
        assert(strcmp(words[2], "world") == 0);
        assert(words[3] == NULL);
        // Ensure in-place pointers into input
        assert(words[0] >= input && words[0] < input + sizeof(input));
        printf("  [PASS] Standard command parsing\n");
    }

    // Test 2: Leading, trailing, and repeated whitespace (spaces, tabs, newlines)
    {
        char input[] = " \t  ls   -la  \t  /tmp \n ";
        res = get_words(input, &wc, words, TEST_MAX_ARGS);
        assert(res == 0);
        assert(wc == 3);
        assert(strcmp(words[0], "ls") == 0);
        assert(strcmp(words[1], "-la") == 0);
        assert(strcmp(words[2], "/tmp") == 0);
        assert(words[3] == NULL);
        printf("  [PASS] Mixed whitespace trimming (tabs, newlines, multiple spaces)\n");
    }

    // Test 3: Single word command
    {
        char input[] = "exit";
        res = get_words(input, &wc, words, TEST_MAX_ARGS);
        assert(res == 0);
        assert(wc == 1);
        assert(strcmp(words[0], "exit") == 0);
        assert(words[1] == NULL);
        printf("  [PASS] Single word command\n");
    }

    // Test 4: Empty string
    {
        char input[] = "";
        res = get_words(input, &wc, words, TEST_MAX_ARGS);
        assert(res == 0);
        assert(wc == 0);
        assert(words[0] == NULL);
        printf("  [PASS] Empty string\n");
    }

    // Test 5: Whitespace-only string
    {
        char input[] = "   \t   \n  ";
        res = get_words(input, &wc, words, TEST_MAX_ARGS);
        assert(res == 0);
        assert(wc == 0);
        assert(words[0] == NULL);
        printf("  [PASS] Whitespace-only string\n");
    }

    // Test 6: Error handling on NULL arguments
    {
        char input[] = "ls -l";
        assert(get_words(NULL, &wc, words, TEST_MAX_ARGS) == -1);
        assert(get_words(input, NULL, words, TEST_MAX_ARGS) == -1);
        assert(get_words(input, &wc, NULL, TEST_MAX_ARGS) == -1);
        printf("  [PASS] NULL pointer parameter checks\n");
    }

    // Test 7: Exceeding max arguments (buffer limit check)
    {
        char input[] = "one two three four five";
        // Pass a limit of 3 (cannot fit 5 words)
        res = get_words(input, &wc, words, 3);
        assert(res == -1);
        printf("  [PASS] Exceeding max arguments check\n");
    }

    printf("All test_get_words cases passed!\n");
    return 0;
}

int run_tests(void) { return test_get_words(); }
