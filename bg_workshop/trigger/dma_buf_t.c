#include "dma_buf_t.h"
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <linux/dma-heap.h>

static int dev_fd = -1;
static inline get_dev_fd()
{
    if(dev_fd>=0)
        return dev_fd;
    dev_fd = open("/dev/dma_heap/system", O_RDWR);
    if(dev_fd<0)
        perror("[-] open system heap");
    return dev_fd;
}

dma_buf_t* dma_buf_create(size_t size)
{
    int dev_fd = get_dev_fd();
    volatile struct dma_heap_allocation_data heap_info = {0};
    heap_info.len = size;
    heap_info.fd_flags = O_RDWR;

    int ret = ioctl(dev_fd, DMA_HEAP_IOCTL_ALLOC, &heap_info);
    if(ret<0)
    {
        perror("[-] system heap ioctl");
        return NULL;
    }

    dma_buf_t* new_buf = malloc(sizeof(*new_buf));
    if(!new_buf)
    {
        perror("[-] malloc dma_buf_t");
        return NULL;
    }
    new_buf->fd = heap_info.fd;
    new_buf->size = heap_info.len;
    return new_buf;
}