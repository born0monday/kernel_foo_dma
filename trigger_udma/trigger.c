#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <linux/memfd.h>
#include <linux/udmabuf.h>
#include <fcntl.h>
#include <sys/io.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define PAGE_SIZE 0x1000
#define errx(ret_code, msg) do{perror(msg); exit(ret_code);} while(0);

int main(int argc, char* argv[])
{
    int mem_fd = memfd_create("test", MFD_ALLOW_SEALING);
    if (mem_fd < 0)
        errx(1, "couldn't create anonymous file");
    
    /* setup size of anonymous file, the initial size was 0 */
    if (ftruncate(mem_fd, PAGE_SIZE) < 0)
        errx(1, "couldn't truncate file length");
    
    /* make sure the file cannot be reduced in size */
    if (fcntl(mem_fd, F_ADD_SEALS, F_SEAL_SHRINK) < 0)
        errx(1, "couldn't seal file");

    int dev_fd = open("/dev/udmabuf", O_RDWR);
    if (dev_fd < 0)
        errx(1, "couldn't open udmabuf");
    
    struct udmabuf_create info = { 0 };
    info.memfd = mem_fd;
    info.size	 = PAGE_SIZE;
    
    /* alloc a `page` array of N_PAGES_ALLOC (i.e. 1 page) */
    int udma_fd = ioctl(dev_fd, UDMABUF_CREATE, &info);
    if (udma_fd < 0)
        errx(1, "couldn't create udmabuf");
    
    /* map the `udmabuf` to userspace with read and write permissions */
    void* addr = mmap(NULL, PAGE_SIZE,
        PROT_READ|PROT_WRITE, MAP_SHARED, udma_fd, 0);
    if (addr == MAP_FAILED)
        errx(1, "couldn't map memory");

    void* remapped_addr = mremap(addr, PAGE_SIZE, PAGE_SIZE+PAGE_SIZE, MREMAP_MAYMOVE);
    if(remapped_addr==MAP_FAILED)
        errx(1, "couldn't mremap memory");

    *((char*)remapped_addr+PAGE_SIZE) = 0;
}