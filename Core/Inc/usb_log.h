#ifndef _USB_LOG_H_
#define _USB_LOG_H_

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

void simple_print(char *str);

#endif
