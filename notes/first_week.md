## First week
We mainly verified that the environment works for everyone and everybody could trigger the bugs.

We then went through the different layers of abstraction to find out how a `dma_buf` is constructed,  
followed by how `mmap`ing the created file then constructs the `vma` with the vulnerable `fault` handler.

Finally, I gave a brief overview of my exploit strategy and some of the things to consider and pitfalls.  
I won't go into the pitfalls here, because we'll go over that in more depth at a later point.

### How is a `dma_buf` constructed
The details depend on the type of dma buf, but in general we perform the following actions.

1. Open the `dma_buf` device driver. 
    * `/dev/udmabuf` or `/dev/dma_heap/system`
2. Set up the arguments for constructing the buffer. 
    * `struct udmabuf_create` or `struct dma_heap_allocation_data`
3. Use an `ioctl` on the device driver `fd` to create a new `fd` for the actual `dma_buf`.
    * `UDMABUF_CREATE` or `DMA_HEAP_IOCTL_ALLOC`

### What are the bugs?
The bugs are both very similar and exist in the `fault` handlers of their respective `dma_buf` types.  

In the case of the `system dma_heap`, the error is in `dma_heap_vm_fault` in `drivers/dma-buf/heap/heap-helpers.c`.

```c
static vm_fault_t dma_heap_vm_fault(struct vm_fault *vmf)
{
	struct vm_area_struct *vma = vmf->vma;
	struct heap_helper_buffer *buffer = vma->vm_private_data;

	if (vmf->pgoff > buffer->pagecount)
		return VM_FAULT_SIGBUS;

	vmf->page = buffer->pages[vmf->pgoff];
	get_page(vmf->page);

	return 0;
}
```

The `vmf->address` field describes the **virtual address that the fault occured at**.

The `vmf->pgoff` field describes the **offset in pages inside the file** that the fault occured in.  
This is basically `(vmf->address - vma->vm_start)/PAGE_SIZE + vma->vm_pgoff`.  
So in our case where we are mapping the buffer with no `pgoff`, it is basically the offset in pages inside the `vma`.

While the `fault` handler performs a bounds check on `vmf->pgoff` it has an off-by-one error.  
Consider a `vma` with only one page. The `buffer->page_count` would be `1`, but both `vmf->page==0` and `vmf->page==1` would pass the test.

In that case `vmf->page = buffer->pages[vmf->pgoff];` would access a `struct page*` that is out-of-bounds of the allocated array.

In the case of `udmabuf` there is **no bounds check** at all.
(You can find more details on the `udmabuf` bug [here](https://labs.bluefrostsecurity.de/blog/cve-2023-2008.html))


### How can we reach these bugs?
There are quite a few layers of indirection, so I will only give a brief overview here.

##### udmabuf
A `udmabuf` sets up its `file_operations` in the `struct miscdevice udmabuf_misc` in `drivers/dma-buf/udmabuf.c`.  
These `fops` only implement an `ioctl` handler. This handler implements the `UDMABUF_CREATE` ioctl that takes a `struct udmabuf_create`.

The data provided in the `udmabuf_create` struct is then translated into a one element long `list` and passed on to the `udmabuf_create` function.

That function performs some checks. (Most relevant are likely maximum size and the requirements for the `shmem` `filp` used for constructing it).  
It then sets up the information for exporting a `dma` buffer, including this `exp_info.ops  = &udmabuf_ops;`

That information is fed into `dma_buf_export` in `drivers/dma-buf/dma-buf.c` which wraps the buffer object in a `dmabuf` fd with `dma_buf_fops` file operations.

When we call `mmap` on a `dma_buf` file descriptor, it will invoke the `mmap` handler in `dma_buf_fops`: `dma_buf_mmap_internal`.  
The internal handler unwrapes the buffer object from the `struct file`s `private_data`, performs a few sanity checks and then invokes the `mmap` handler of the unwrapped buffer, in our case: `mmap_udmabuf`

`mmap_udmabuf` then sets up the `vma` with `vm_ops = &udmabuf_vm_ops`. And those `udmabuf_vm_ops` are what contains the vulnerable fault handler.

If we cause any page fault on a virtual address that lies within a `vma` the `fault` handler will be invoked (if it exists) to handle the fault.


##### system dma_heap
The system dma_heap functions slightly differently.  

`system_heap_create` in `drivers/dma-buf/heap/system_heap.c` is called when the `module` is initialized and will set up a `dma_heap` type device with a `system_heap` subtype using `dma_add_heap` in `drivers/dma-buf/dma-heap.c` with `system_heap_ops` operations.  

This creates a new virtual file in the device tree at `/dev/dma-heap/system` with `dma_heap_fops` `file_operations`.
Opening this new file will invoke `dma_heap_open` which will look up the `system` heap and initialize the new files private data with the type of the `system_heap`.

The only other operation implemented by this virtual file is `ioctl` which implements a `DMA_HEAP_IOCTL_ALLOC` that takes a `struct dma_heap_allocation_data` as input.

Calling this `ioctl` will construct a new file descriptor in `dma_heap_buffer_alloc` which then invokes the heap subtypes `allocate` handler.  
In our case `system_heap_allocate` in `drivers/dma-buf/heap/system_heap.c`.

This sets up a `struct heap_helper_buffer` and calls `heap_helper_export_dmabuf` to construct the new file descriptor for the buffer.  
That function is a small wrapper around `dma_buf_export`, which sets up `heap_helper_ops` file operations. These file operations contain the vulnerable `fault` handler with the off-by-one error.

As with udmabuf, if we cause any page fault on a virtual address that lies within a `vma` the `fault` handler in `vma->vm_ops` will be invoked (if it exists).


### How can we trigger these bugs?
The `vma` that we create with `mmap` has to have the correct size or the `mmap` call will fail.  

So even if `vmf->pgoff` could trigger the bug, we have no way of `mmap`ing a `vma` that is larger than the allocated number of pages.  
At first glance this seems to prevent us from using the bug(s) at all.

But the `vma` doesn't set the `VM_DONTEXPAND` flag, which would prevent it from growing.  
So `mremap` can be used to make the `vma` larger.

This allows us to cause a page fault at an address large enough that it doesn't correspond to any page in the `vma`/`dma_buf` anymore.

### What happens when we trigger the bugs?
If you don't do anything special you will very likely crash.  
The `fault` handler expects the OOB access to return a pointer to a `struct page` object.  
If that pointer isn't a valid address it will cause a crash in this case probably just terminating your exploit process.  
In general, in the worst case you might crash all the way to a kernel panic or freezing a cpu core.

If the pointer is valid, but isn't to a `struct page*` you will very likely also crash. The kernel has a special virtual address range reserved for pages.  
If your pointer is outside of that address range, the calculation for the corresponding physical address will go wrong and will probably cause an invalid access down the line anyway.

If the pointer is valid and a pointer to a `struct page` the `fault` handler will take an extra reference to the page and return it. `finish_fault` in `mm/memory.c` will then insert that `page` into your processes address space without any additional checks.

So if everything goes well you get a page of kernel memory mapped into your process and can now read and write in it.