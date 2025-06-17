
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <error.h>
#include <linux/memfd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include "memfd_t.h"

memfd_t * memfd_t_create(size_t size)
{
    memfd_t *new_memfd = malloc(sizeof(*new_memfd));
    if(!new_memfd)
    {
        
        return NULL;
    }

    new_memfd->size = size;
    new_memfd->fd = memfd_create("test", MFD_ALLOW_SEALING);
    if (new_memfd->fd < 0)
    {
        perror("couldn't create anonymous file");
        goto out_free;
    }
    
    /* setup size of anonymous file, the initial size was 0 */
    if (ftruncate(new_memfd->fd, new_memfd->size) < 0)
    {
        perror("couldn't truncate file length");
        goto out_free;
    }
    
    /* make sure the file cannot be reduced in size */
    if (fcntl(new_memfd->fd, F_ADD_SEALS, F_SEAL_SHRINK) < 0)
    {
        perror("couldn't seal file");
        goto out_free;
    }

    return new_memfd;

    out_free:
    free(new_memfd);
    return NULL;
}