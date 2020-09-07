// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define PRIME 49

struct {
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // head.next is most recently used.
} bcache;

struct buf hashtable[PRIME];
struct spinlock hashlock[PRIME]; //protects refcnt

void
printBuf(struct buf *b, char *info)
{
  if(!b){
    printf("buf == null\n");
    return;
  }
  printf("%s: ", info);
  printf("No %d, %d, %d, %d, %d, %d\n", b - bcache.buf, b->dev, b->blockno, b->refcnt, b->used, b->timestamp);
}

void
printBufList(char *info)
{
  struct buf *b;
  printf("%s\n", info);
  printf("No, dev, blockno, refcnt, used, timestamp\n");
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    printf("%d %d, %d, %d, %d, %d\n", b - bcache.buf, b->dev, b->blockno, b->refcnt, b->used, b->timestamp);
  }
}


void
assignBuf(struct buf *b, uint dev, uint blockno)
{
  if(!b) return;
  b->dev = dev;
  b->blockno = blockno;
  b->valid = 0;
  b->refcnt = 1;
  b->used = 1;
  b->timestamp = ticks;
}

void
addList(struct buf *head, struct buf *b)
{
  if(!head || !b) return;
  b->prev = head;
  b->next = head->next;
  head->next = b;
  b->next->prev = b;
}

void
removeList(struct buf *b){
  if(!b) return;
  b->prev->next = b->next;
  b->next->prev = b->prev;
}


int
calcHash(uint dev, uint blockno){
  //uint64 res = (dev + blockno);
  //res *= res;
  //res %= PRIME;
  return (dev + blockno) % PRIME;
}

void
binit(void)
{
  struct buf *b;

  // init buf
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    initsleeplock(&b->lock, "buffer");
    b->used = 0;
  }

  // init hashtable
  for(int i = 0; i < PRIME; ++i){
    initlock(&hashlock[i], "bcache.hashlock");
    hashtable[i].next = &hashtable[i];
    hashtable[i].prev = &hashtable[i];
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int hash_index = calcHash(dev, blockno);
  //printBufList("pre-bget");
  // Is the block already cached?
  acquire(&hashlock[hash_index]);
  //printf("got hashlock\n");
  struct buf *head = &hashtable[hash_index];
  b = head->next;
  while(b != head){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&hashlock[hash_index]);
      acquiresleep(&b->lock);
      return b;
    }
    b = b->next;
  }
  // Still holding hashlock[hash_index]
  
  // Not cached; recycle an unused buffer.
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    if(__sync_bool_compare_and_swap(&b->used, 0, 1)){
      assignBuf(b, dev, blockno);
      addList(&hashtable[hash_index], b);
      release(&hashlock[hash_index]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&hashlock[hash_index]);
  
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b->dev, b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b->dev, b, 1);
}

// Release a locked buffer.
// Move to the head of the MRU list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int hash_index = calcHash(b->dev, b->blockno);
  // Only modifying refcnt (instead of adding/removing entries)
  // No need for bcache.lock
  // The fact that brelse(struct buf *b) was called ensures the existence of b in hashtable[hash_index] 
  // and that *b is protected by hashlock[hash_index]
  acquire(&hashlock[hash_index]);
  b->refcnt--;
  if(b->refcnt == 0){
    // remove b from hashtable
    // mark b unused
    removeList(b);
    b->used = 0;
  }
  release(&hashlock[hash_index]);
}

// If bpin and bunpin are not holding bcache.lock, nasty as every funtion is,
// bget might evict *b (i.e. changing b->dev, b->blockno, etc)
// between [int hash_index = calcHash(b->dev, b->blockno);]
// and [acquire(&hashlock[hash_index]);]
// But will acquiring bcache.lock then hashlock[hash_index] deadlock with bget?
// Unless some jerk calls bpin/bunpin when b->refcnt == 0!
// If *b are in hashtable, then bget will only acquire hashlock[hash_index] then return, so no;
// However, if *b
void
bpin(struct buf *b) {
  int hash_index = calcHash(b->dev, b->blockno);
  acquire(&hashlock[hash_index]);
  b->refcnt++;
  release(&hashlock[hash_index]);
}

void
bunpin(struct buf *b) {
  int hash_index = calcHash(b->dev, b->blockno);
  acquire(&hashlock[hash_index]);
  b->refcnt--;
  release(&hashlock[hash_index]);
}