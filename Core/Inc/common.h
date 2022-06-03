#ifndef _COMMON_H_
#define _COMMON_H_

#define count_of(arr) (sizeof(arr) / sizeof(arr[0]))
#define align_up(x, a) (((x) + (a) - 1) / (a))

#define container_of(ptr, type, member) ({                \
    const typeof(((type *)0)->member) *__mptr = (ptr);    \
    (type *)((char *)__mptr - offsetof(type, member));})

#endif