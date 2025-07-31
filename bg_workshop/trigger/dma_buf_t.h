#ifndef __DMA_BUF_T__
#define __DMA_BUF_T__
#include <stddef.h>
typedef struct
{
    int fd;
    size_t size;
} dma_buf_t;

dma_buf_t* dma_buf_create(size_t size);
#endif