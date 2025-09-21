## Kernel-Foo dma buf

This repo contains the env and a trigger and I'll gradually add more stuff as we go.

The env is a modified version of https://github.com/MyEyes/basic_linux_env

The kernel is a 5.10.226 mainline kernel in default config, except that:
* I've enabled useful debug features
* I've enabled 9p so a host directory can be mounted
* I've enabled the features we are targetting.

The two bugs we will look at are:
* bug in udmabuf https://labs.bluefrostsecurity.de/blog/cve-2023-2008.html
* bug in system dma-heap https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git/commit/?id=007180fcb6cc4a93211d4cc45fef3f5ccccd56ae

I've included both the `bzImage` and the `vmlinux` ELF file, just for convenience.

Running any of the triggers should likely cause a kernel BUG print.

### Special thanks

I want to thank `bam0x7` for asking about this CVE in the kctf discord server.  
Our exchange there inspired me to do this workshop in the first place and it was fun to work with him to help him finish his own exploit.