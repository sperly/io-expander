/* Minimal unit test helpers. */
#ifndef UNIT_H
#define UNIT_H

#include <stdio.h>

extern int unit_failures, unit_checks;

#define CHECK(cond)                                                                    \
    do {                                                                               \
        unit_checks++;                                                                 \
        if (!(cond)) {                                                                 \
            unit_failures++;                                                           \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                   \
        }                                                                              \
    } while (0)

#define CHECK_EQ(a, b)                                                                 \
    do {                                                                               \
        long long _a = (long long)(a), _b = (long long)(b);                            \
        unit_checks++;                                                                 \
        if (_a != _b) {                                                                \
            unit_failures++;                                                           \
            printf("  FAIL %s:%d: %s == %s (%lld != %lld)\n", __FILE__, __LINE__, #a, #b, _a, _b); \
        }                                                                              \
    } while (0)

#define RUN(fn)                                                                        \
    do {                                                                               \
        printf("%s\n", #fn);                                                           \
        fn();                                                                          \
    } while (0)

#endif
