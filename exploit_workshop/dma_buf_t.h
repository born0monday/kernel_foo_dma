#ifndef DMA_BUF_T_H
#define DMA_BUF_T_H
#include <stddef.h>

typedef int dma_buf_fd_t;

typedef struct {
    dma_buf_fd_t buf_fd;
    size_t size;
} dma_buf_t;

dma_buf_t* dma_buf_create(size_t size);
#endif