// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

struct {
  struct spinlock lock;
  int refcnt[MaxIndex+10];
} memref;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&memref.lock,"memref");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.

  acquire(&memref.lock);
  if(memref.refcnt[MemIndex((uint64)pa)] > 1){
    memref.refcnt[MemIndex((uint64)pa)]-=1;
    release(&memref.lock);
    return;
  }
  release(&memref.lock);
  
  r = (struct run*)pa;
  memset(pa,1,PGSIZE);
  
  acquire(&memref.lock);
  memref.refcnt[MemIndex((uint64)pa)] = 0;
  release(&memref.lock);

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);

}

void
incre_mem_ref(uint64 pa)
{
  acquire(&memref.lock);
  // printf("incre pa:%p  %d\n",pa,memref.refcnt[MemIndex(pa)]);
  release(&memref.lock);
  acquire(&memref.lock);
  memref.refcnt[MemIndex(pa)]++;
  // printf("incre pa after:%p  %d\n",pa,memref.refcnt[MemIndex(pa)]);
  release(&memref.lock);
}

void 
decre_mem_ref(uint64 pa)
{
  if(pa > PHYSTOP || pa == 0) return;
  kfree((void *)pa);
}


// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r){
    memset((char*)r, 5, PGSIZE); // fill with junk
    // 对应引用设置为1
    acquire(&memref.lock);
    memref.refcnt[MemIndex((uint64)r)] = 1;
    // printf("kalloc pa:%p  %d\n",r,memref.refcnt[MemIndex((uint64)r)]);
    release(&memref.lock);
  }
    
  return (void*)r;
}
