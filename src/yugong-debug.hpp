#ifndef __YUGONG_DEBUG__
#define __YUGONG_DEBUG__

#include <cstdio>

#ifdef YG_DEBUG
#define yg_debug(fmt, ...) fprintf(stdout, "YG DEBUG [%s:%d:%s] " fmt,\
        __FILE__, __LINE__, __func__, ## __VA_ARGS__)
#else
#define yg_debug(fmt, ...)
#endif // YG_DEBUG

#endif // __YUGONG_DEBUG__
