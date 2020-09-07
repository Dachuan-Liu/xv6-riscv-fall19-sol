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
  uint64 npages;
} kmem[NCPU];

void
printKnem(char *str)
{
  printf("%s\n", str);
  int total = 0;
  for(int i = 0; i < NCPU; ++i){
    total += kmem[i].npages;
    printf("Core %d: %d\n", i, kmem[i].npages);
  }
  printf("total: %d\n", total);
}

void
kinit()
{
  //printKnem("before init");
  for(int i = 0; i < NCPU; ++i){
    initlock(&kmem[i].lock, "kmem");
    kmem[i].freelist = 0;
    kmem[i].npages = 0;
  }
  //printKnem("before freerange");
  freerange(end, (void*)PHYSTOP);
  //printKnem("after init");
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
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;
  push_off();
  int id = cpuid();
  acquire(&kmem[id].lock);
  r->next = kmem[id].freelist;
  kmem[id].freelist = r;
  kmem[id].npages += 1;
  release(&kmem[id].lock);
  pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  //printKnem("before kalloc");
  
  const int page_unit = 8;
  struct run *r;
  struct run *stolen_head = 0, *stolen_tail = 0;
  int toSteal = 0;

  push_off();
  int id = cpuid();
  acquire(&kmem[id].lock);
  r = kmem[id].freelist;
  if(r){
    kmem[id].freelist = r->next;
    kmem[id].npages -= 1;
    release(&kmem[id].lock);
  }
  else{
    release(&kmem[id].lock);
    // find victim CPU with free pages
    for(int i = 0; i < NCPU; ++i){
      //r = stolen_head = stolen_tail = 0;
      //toSteal = 0;
      acquire(&kmem[i].lock);
      if(kmem[i].npages > 0){
        r = kmem[i].freelist;
        kmem[i].freelist = kmem[i].freelist->next;
        kmem[i].npages -= 1;
      }
      if((i != id) && kmem[i].npages > page_unit * 2){ // only steal other CPU with adequate free pages
        toSteal = page_unit;
        stolen_head = stolen_tail = kmem[i].freelist;
        for(int j = 0; j < toSteal - 1; ++j){ // steal [stolen_head, stolen_tail]
          stolen_tail = stolen_tail->next;
        }
        kmem[i].npages -= toSteal;
        kmem[i].freelist = stolen_tail->next;
        stolen_tail->next = 0;
      }
      release(&kmem[i].lock);
      if(r) break; // found victim
    }
  }

  if(toSteal > 0){
    acquire(&kmem[id].lock);
    stolen_tail->next = kmem[id].freelist;
    kmem[id].freelist = stolen_head;
    kmem[id].npages += toSteal;
    release(&kmem[id].lock);
    //printKnem("after stealing");
  }
  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  pop_off();
  //printf("allocated %p from %d\n", (void*)r, id);
  return (void*)r;
}
