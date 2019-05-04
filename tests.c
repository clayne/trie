#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "trie.h"

#define EXP_TOTAL 5
#define EXP_WORDS 11

static void
die(const char *s)
{
    fprintf(stderr, "%s\n", s);
    exit(EXIT_FAILURE);
}

static uint32_t
pcg32(uint64_t s[1])
{
    uint64_t h = s[0];
    s[0] = h * UINT64_C(0x5851f42d4c957f2d) + UINT64_C(0xd737232eeccdf7ed);
    uint32_t x = ((h >> 18) ^ h) >> 27;
    unsigned r = h >> 59;
    return (x >> r) | (x << (-r & 31u));
}

static char *
generate(uint64_t *s)
{
    int min = 8;
    int max = 300;
    int len = min + pcg32(s) % (max - min);
    char *key = malloc(len + 1);
    if (!key)
        die("out of memory");
    for (int i = 0; i < len; i++)
        key[i] = 1 + pcg32(s) % 255;
    key[len] = 0;
    return key;
}

static void *
replacer(const char *key, void *value, void *arg)
{
    (void)key;
    free(value);
    return arg;
}

static void
hexprint(const char *label, const char *s)
{
    unsigned char *p = (unsigned char *)s;
    fputs(label, stdout);
    for (size_t i = 0; *p; i++)
        printf("%c%02x", i % 16 ? ' ' : '\n', *p++);
    putchar('\n');
}

static int
check_order(const char *key, void *value, void *arg)
{
    char **prev = arg;
    if (*prev && strcmp(*prev, key) >= 0) {
        hexprint("first:", *prev);
        hexprint("second:", key);
        die("FAIL: keys not ordered");
    }
    *prev = value;
    return 0;
}

int
main(void)
{
    uint64_t rng = 0xabf4206f849fdf21;

    struct trie *t = trie_create();
    for (int i = 0; i < 1 << EXP_TOTAL; i++) {
        long count = (1L << EXP_WORDS) + pcg32(&rng) % (1L << EXP_WORDS);
        uint64_t rngsave = rng;

        for (long j = 0; j < count; j++) {
            char *key = generate(&rng);
            if (trie_insert(t, key, key))
                die("out of memory");
        }

        /* Check that all keys are present. */
        uint64_t rngcopy = rngsave;
        for (long j = 0; j < count; j++) {
            char *key = generate(&rngcopy);
            char *r = trie_search(t, key);
            if (!r)
                die("FAIL: missing key");
            if (strcmp(r, key))
                die("FAIL: value mismatch");
            free(key);
        }

        char *prev = 0;
        if (trie_visit(t, "", check_order, &prev))
            die("out of memory");

        /* Remove all entries. */
        rngcopy = rngsave;
        for (long j = 0; j < count; j++) {
            char *key = generate(&rngcopy);
            if (trie_replace(t, key, replacer, 0))
                die("out of memory");
            free(key);
        }

        /* Check that all keys are gone. */
        rngcopy = rngsave;
        for (long j = 0; j < count; j++) {
            char *key = generate(&rngcopy);
            char *r = trie_search(t, key);
            if (r)
                die("FAIL: key not removed");
            free(key);
        }

        /* Print out current trie size (as progress) */
        double mb = trie_size(t) / 1024.0 / 1024.0;
        printf("%-2d trie_size() = %10.3f MiB\n", i, mb);
    }
    trie_free(t);
}
