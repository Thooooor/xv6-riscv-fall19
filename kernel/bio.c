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

#define HashSize 17
struct {
  struct spinlock lock[HashSize];
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // head.next is most recently used.
  // struct buf head;
  struct buf hashbucket[HashSize];
} bcache;

int hashconflict(int number) {
  return (number + 1) % HashSize;
}

void
binit(void)
{
  struct buf *b;
  // initial every lock
  for (int i = 0; i < HashSize; i++) {
    char *lockname = "bcache 00";
    initlock(&bcache.lock[i], lockname);
    // Create linked list of buffers
    bcache.hashbucket[i].prev = &bcache.hashbucket[i];
    bcache.hashbucket[i].next = &bcache.hashbucket[i];
  }
  // use bcache.hashbucket[0] to store temporally， made a mistake there (put it in the loop above)
  for (b = bcache.buf; b < bcache.buf+NBUF; b++) {
    b->next = bcache.hashbucket[0].next;
    b->prev = &bcache.hashbucket[0];
    initsleeplock(&b->lock, "buffer");
    bcache.hashbucket[0].next->prev = b;
    bcache.hashbucket[0].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  // get the hash index of blockno
  int locknumber = blockno % HashSize;
  struct buf *b;
  acquire(&bcache.lock[locknumber]);

  // Is the block already cached?
  for(b = bcache.hashbucket[locknumber].next; b != &bcache.hashbucket[locknumber]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock[locknumber]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  int hashnumber = hashconflict(locknumber);
  // Not cached; recycle an unused buffer.
  while (hashnumber != locknumber) {
    acquire(&bcache.lock[hashnumber]);
    for(b = bcache.hashbucket[hashnumber].prev; b != &bcache.hashbucket[hashnumber]; b = b->prev){
      if(b->refcnt == 0) {
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
        // take out the buf
        b->next->prev = b->prev;
        b->prev->next = b->next;

        release(&bcache.lock[hashnumber]);
        // put the buf in locknumber bcache
        b->next = bcache.hashbucket[locknumber].next;
        b->prev = &bcache.hashbucket[locknumber];
        bcache.hashbucket[locknumber].next->prev = b;
        bcache.hashbucket[locknumber].next = b;

        release(&bcache.lock[locknumber]);
        acquiresleep(&b->lock);
        return b;
      }
    }
    release(&bcache.lock[hashnumber]);
    hashnumber = hashconflict(hashnumber);
  }
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
  uint blockno = b->blockno;
  int locknumber = blockno % HashSize;
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  acquire(&bcache.lock[locknumber]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.hashbucket[locknumber].next;
    b->prev = &bcache.hashbucket[locknumber];
    bcache.hashbucket[locknumber].next->prev = b;
    bcache.hashbucket[locknumber].next = b;
  }
  release(&bcache.lock[locknumber]);
}

void
bpin(struct buf *b) {
  uint blockno = b->blockno % HashSize;
  acquire(&bcache.lock[blockno]);
  b->refcnt++;
  release(&bcache.lock[blockno]);
}

void
bunpin(struct buf *b) {
  uint blockno = b->blockno % HashSize;
  acquire(&bcache.lock[blockno]);
  b->refcnt--;
  release(&bcache.lock[blockno]);
}


