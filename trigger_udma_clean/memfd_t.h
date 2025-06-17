#ifndef MEMFD_T_H
#define MEMFD_T_H
#include <stddef.h>
typedef int memfd_fd_t;
typedef struct{
    memfd_fd_t fd;
    size_t size;
} memfd_t;

memfd_t * memfd_t_create(size_t size);
#endif