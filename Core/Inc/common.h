#ifndef _COMMON_H_
#define _COMMON_H_
#include <stdio.h>

#ifdef DEBUG
#define log_info(fmt, args...) printf("%s[%d]: " fmt, __func__, __LINE__, ##args)
#else
#define log_info(fmt, args...)
#endif
#define log_err(fmt, args...) printf("%s[%d]: " fmt, __func__, __LINE__, ##args)

#define count_of(arr) (sizeof(arr) / sizeof(arr[0]))

#endif