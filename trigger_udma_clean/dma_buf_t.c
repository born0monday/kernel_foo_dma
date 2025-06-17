#define _GNU_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <error.h>
#include <stdlib.h>
#include <linux/udmabuf.h>
#include <sys/ioctl.h>
#include <sys/io.h>
#include <sys/mman.h>
#include <fcntl.h>
#include "dma_buf_t.h"

static int __dev_fd = -1;
static inline int __get_dev_fd()
{
    if(__dev_fd>=0)
        return __dev_fd;

    __dev_fd = open("/dev/udmabuf", O_RDWR);
    if (__dev_fd < 0)
        perror("[-] couldn't open udmabuf");
    return __dev_fd;
}

dma_buf_t * dma_buf_create(memfd_t* memfd)
{
    dma_buf_t* new_buf = malloc(sizeof(*new_buf));
    if(!new_buf)
        return NULL;

    struct udmabuf_create info = { 0 };
    info.memfd = memfd->fd;
    info.size = memfd->size;

    int dev_fd = __get_dev_fd();
    
    /* alloc a `page` array of N_PAGES_ALLOC (i.e. 1 page) */
    int udma_fd = ioctl(dev_fd, UDMABUF_CREATE, &info);
    if (udma_fd < 0)
    {
        perror("[-] couldn't create udmabuf");
        free(new_buf);
        return NULL;
    }

    new_buf->buf_fd = udma_fd;
    new_buf->size = memfd->size;
    new_buf->memfd = memfd;
    return new_buf;
}