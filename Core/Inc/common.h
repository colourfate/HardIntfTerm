#ifndef _COMMON_H_
#define _COMMON_H_
#include <stdio.h>

#define DEBUG
#ifdef DEBUG
#define log_info(fmt, args...) printf("%s[%d]: " fmt, __func__, __LINE__, ##args)
#define log_info_raw(fmt, args...) printf(fmt, ##args);
#else
#define log_info(fmt, args...)
#define log_info_raw(fmt, args...)
#endif
#define log_err(fmt, args...) printf("%s[%d]: " fmt, __func__, __LINE__, ##args)

#define count_of(arr) (sizeof(arr) / sizeof(arr[0]))
#define align_up(x, a) (((x) + (a) - 1) / (a))

#endif