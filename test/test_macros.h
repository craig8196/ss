
#ifndef TEST_MACROS_H_
#define TEST_MACROS_H_
#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#define log_test() \
    { \
        printf("Run test: %s\n", __func__); \
    }

#ifdef __cplusplus
}
#endif
#endif /* TEST_MACROS_H_ */


