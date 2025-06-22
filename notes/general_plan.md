## Context
The kernel we are doing this on is a stock 5.10.226 mainline kernel.  
(I had to modify objtool and some makefiles to be able to build on a modern compiler, but the kernel code itself is untouched)

In this version both the `udmabuf` bug and the `dma_heap` bug exist.

I'm using a default config with the following additional things enabled:
* debugfs and ftrace - for easier introspection
* 9p and virtio - to mount a host directory
* dma system heap - so that the second bug is available.

Additionally, I am changing the permissions of `/dev/dma_heap/system` in the `init` script,  
so that a low priv user can access the device easily. (You could also do this if unprivileged user namespaces are available)

## General Plan
I would like to take my time with this and eventually compile it into a comprehensive series of blog posts  
about going from a bug to a full exploit on a modern kernel.

Both bugs give a use-after-free (UAF) of a `struct page*` in the kernel, but have slightly different parameters around it.  
My current plan is to break this down into several steps, which maybe will have to be refined further:

1. What are the bugs and why is this an issue?
    * What's a `vma`?
    * What are the different `VM_X` flags?
    * How can we interact with a `vma`?
    * How do fault handlers work?
2. How can we interact with the kernel and reach these bugs?
    * System calls
    * File operations
    * vm_ops
    * page faults/fault handlers
3. How do we avoid crashing/get a `page*` into the OOB read spot
    * Heap spraying generally
    * Slab allocator
    * Kmalloc slabs
4. What can we do with a mapped page?
    * Clarifying the primitive(s). (Difference between UAF of `struct page` and UAF of `struct page*`)
    * When does a page become use after free/Reference counting
    * What are some standard spray candidates. (ELOISE paper/well known primitives)
    * Interpreting data we read from a stolen page
5. What do we want to achieve?
    * Kernel mitigations
    * Bypassing kernel mitigations
    * Different common ways for LPE (overwrite modprobe_path, overwrite creds, make read-only file writeable)
    * Advantages and disadvantages
    * Creating a full exploit.
6. Improving the exploit.
    * What may lead to the exploit crashing?
    * What may lead to the exploit not crashing, but also not working?
    * General structure to make retrying easy
    * Other considerations