## Third week
The main goal was to see if we can get the page that we stole to contain some interesting information.  
We first did a basic spray and then adjusted the way we spray to make it more likely that we get a page we are interested in.  
This also made us run into an unexpected issue that I'll document down below.

## Spraying the `buffer->pages` arrays
The first part of the spray is to get a reference to a page that won't be altered while we use it, but also point to a page that is free.

The `buffer->pages` array is allocated by calling `kmalloc_array` for the requested number of page pointers, but without the `__GFP_ZERO` flag,  
that would cause the full allocation to be zeroed out.

Because of the way the slab allocator works, only certain sizes of memory allocations can be returned from `kmalloc`  
and if the requested size doesn't exactly match it is rounded up to make sure it fits.

This means that if we allocate an array of **8** `struct page*` we will get the allocation from the `kmalloc-64` slab cache.  
Asking for **7** `struct page*` instead will still give us an allocation from the `kmalloc-64` slab cache, just with the last 8 byte being unused.

The idea of the spray is to create a bunch of these arrays that contain valid pointers to `struct page` structures and then free them again.  
In the example of requesting **8** pages we expect something like this:

*for simplicity I'm just using indices for the pages instead of full pointers, but consider every index to uniquely belong to a `struct page` somewhere in kernel memory*
```
spray_array[0]->(kernel side)->pages = [00|01|02|03|04|05|06|07]
spray_array[1]->(kernel side)->pages = [08|09|10|11|12|13|14|15]
[...]
```

When we free the first array of pages, the pages themselves are returned to the buddy allocator (more on that in a bit)  
and the array itself is returned to the `kmalloc-64` slab cache.

If we now allocate a new array of pages with only **7** entries we expect the following, assuming the new pages we get start at index 42:

```
new_array->(kernel side)->pages = [42|43|44|45|46|47|48|07]
```

The kernel will overwrite all of the requested `struct page*` entries with new references, but we are left with one *stale* entry to the page that was previously allocated.

In code this would look roughly like this:

```c
int spray()
{
    for(int i=0; i<NUM_SPRAYS; i++)
    {
        for(int j=0; j<NUM_SPRAY_BUFS; j++)
        {
            spray_array[j] = dma_buf_create(NUM_SPRAY_PAGES*PAGE_SIZE);
        }

        for(int j=0; j<NUM_SPRAY_BUFS; j++)
        {
            close(spray_array[j]->buf_fd);
            free(spray_array[j]);
            spray_array[j] = NULL;
        }
    }
}

int main(int argc, char* argv[])
{
    spray();

    printf("[+] creating dma buf\n");
    dma_buf_t *dma_buf = dma_buf_create((NUM_SPRAY_PAGES-1)*PAGE_SIZE);
    if(!dma_buf)
        errx(1, "[-] couldn't create dma buf");
```

So this is the general idea, but we need to be a little more thoughtful about it.

## Which pages do we get in the spray?
The `page allocator` or `buddy allocator` has a separate `pcp_freelist` on every CPU for performance reasons.  
If we free a page it gets added to the start of that freelist. (see `free_unref_page_commit`)  
If we request a page and there is such a page on the freelist, we also retrieve it from the start of the list (see `__rmqueue_pcplist`).  

So in that way the `list` acts more like a `stack` that pages get *pushed* and *poped* on and off.

What this means in practice is that in the above scenario we would expect the following to be what actually happens:
```
1. spray
   spray_array[0]->(kernel side)->pages = [00|01|02|03|04|05|06|07]
2. free pages array
3. allocate new array
   new_array->(kernel side)->pages = [07|06|05|04|03|02|01|07]
```

So in that case we haven't really won anything, because we have access to `page #7` anyway.  
The easiest way to get around this is to make sure the most recently freed pages are used for something else,  
but at the same time we need to be a little careful not to ruin our spray of the pointers in the `kmalloc-64` slabs.

We can just create several `dma_buf`s that land in another `kmalloc` slab and use up the pages we previously freed.
Let's say we spray an allocation that is twice as big for simplicity.

We expect the following:

```
1. spray 8 page arrays:
   spray8_array[0]->(kernel side)->pages = [00|01|02|03|04|05|06|07]
   spray8_array[1]->(kernel side)->pages = [08|09|10|11|12|13|14|15]
2. free arrays:
   free objects in kmalloc-64:
   [00|01|02|03|04|05|06|07] [08|09|10|11|12|13|14|15]
3. spray 16 page arrays:
   allocated object in kmalloc-128
   spray16_array[0]->[15|14|13|12|11|10|09|08|07|06|05|04|03|02|01|00]
4. allocate new array with 7 pages
   new_array->(kernel side)->pages = [16|17|18|19|20|21|22|07]
5. free 16 page array
   new_array->(kernel side)->pages = [16|17|18|19|20|21|22|07] <- now 07 is back in buddy allocator
```

This setup will work to get us a page that is free to be used for anything in the kernel.

Here it is in c, up to step 4
```c
int drain()
{
    for(int j=0; j<NUM_DRAIN_BUFS; j++)
    {
        spray_array[j] = dma_buf_create(2*(NUM_SPRAY_PAGES)*PAGE_SIZE);
    }
}

int main(int argc, char* argv[])
{
    spray();

    printf("[+] draining\n");
    drain();

    printf("[+] creating dma buf\n");
    dma_buf_t *dma_buf = dma_buf_create((NUM_SPRAY_PAGES-1)*PAGE_SIZE);
    if(!dma_buf)
        errx(1, "[-] couldn't create dma buf");
```

Another benefit is that until we do step 5 above, everything is *"frozen"* in place.  
The reference to page 07 won't change, because the kernel doesn't use those last 8 bytes of our `kmalloc-64` allocation for anything  
and because we are also holding the 16 page array ourselves we can control when the page is freed.

## Unexpected failures
We ran into some issues I didn't anticipate during the workshop and I went and found out what happened later.

When we trigger the fault handler by trying to access `page #07` in the above example, the fault handler takes an extra reference to the page. (See `dma_heap_vm_fault`)

When we use `madvice(addr, len, MADV_DONTNEED)` to unmap the page this reference is dropped again.
*(I can't give a simple reference here, because the kernel batches the freeing of pages together with tlb invalidation and it's a little convoluted)*

If the last reference is dropped the page is fully freed and is added to the `pcp_freelist` (See `free_unref_page_commit`).  

But because we *steal* the page we don't know what state it's in, so it may already be on the `pcp_freelist`.  
Removing a list entry poisons that entries `prev` and `next` pointers. (See `list_del` in `list.h`)

So if our entry is on the list already we will likely corrupt the list.

```
1. start state
    pcp_freelist->Page_A->Page_B->Page_C->pcp_freelist
2. enqueue Page_C a second time
    pcp_freelist->Page_C->Page_A->Page_B->Page_C->Page_A->...
3. rmqueue Page_C
    pcp_freelist->Page_A->Page_B->Page_C->LIST_POISON1
```

When that happens we will crash at a later point whenever an allocation tries to remove the second `Page_C` entry from the `pcp_freelist`.  
As you can imagine, this is very annoying to track down.

**To avoid this issue make sure the page you steal is already in use when you cause the page fault**

## Scan loop
I'll probably cover this in a bit more depth next week.
The general idea is that we spray kernel objects and then scan our stolen page to see if we found something.
Because of the above issue we may need to cover this in detail, but the general idea is very basic.

```
1. spray
2. check if interesting structure
2.1. if found exit scan loop
3. free spray
4. repeat
```

We were using `signal_fd` as our spray object, but it could be basically anything that gives a nice primitive.
Details on `signal_fd` are also next week or can be found in this Project Zero writeup: https://googleprojectzero.blogspot.com/2022/11/a-very-powerful-clipboard-samsung-in-the-wild-exploit-chain.html