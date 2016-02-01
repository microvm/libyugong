#ifndef __TEST_COMMON__
#define __TEST_COMMON__

#define ygt_print(fmt, ...) fprintf(stdout, "[%d:%s] " fmt,\
        __LINE__, __func__, ## __VA_ARGS__)

#endif // __TEST_COMMON__
