#ifndef _COMMON_H_
#define _COMMON_H_
#include <stdio.h>

#define log_err(fmt, args...) printf("%s[%d]: " fmt, __func__, __LINE__, ##args)

#endif