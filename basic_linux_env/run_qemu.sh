#!/bin/sh
qemu-system-x86_64 \
    -m 2048\
    -kernel bzImage\
    -append 'console=ttyS0 noapic'\
    -initrd basic_initfs/initramfs\
    -virtfs local,path=./host,mount_tag=host,security_model=passthrough,id=host\
    -nographic
    #-serial mon:stdio