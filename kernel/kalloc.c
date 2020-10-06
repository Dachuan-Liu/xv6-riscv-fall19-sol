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
  uint *ref_count;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  kmem.ref_count = (uint*)end;
  uint64 numPages = (PGROUNDUP(PHYSTOP - (uint64)end)) / PGSIZE;
  int offset = PGROUNDUP(numPages * sizeof(uint));
  memset(end, 0, offset);
  freerange(end + offset, (void*)PHYSTOP);
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

  uint64 idx = ((uint64)pa - (uint64)end) / PGSIZE;
  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  kmem.ref_count[idx] = 0;
  release(&kmem.lock);
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
  if(r){
    uint64 idx = ((uint64)r - (uint64)end) / PGSIZE;
    kmem.ref_count[idx] = 1;
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

uint
kgetref(void *pa)
{
  uint64 idx = ((uint64)pa - (uint64)end) / PGSIZE;
  return kmem.ref_count[idx];
}

void
kref(void *pa)
{
  uint64 idx = ((uint64)pa - (uint64)end) / PGSIZE;
  acquire(&kmem.lock);
  ++kmem.ref_count[idx];
  release(&kmem.lock);
}

void
kderef(void *pa)
{
  uint64 idx = ((uint64)pa - (uint64)end) / PGSIZE;
  int shall_free = 0; // will deadlock without it
  acquire(&kmem.lock);
  if(--kmem.ref_count[idx] == 0)
    shall_free = 1;
  release(&kmem.lock);
  if(shall_free)
    kfree(pa);
}
