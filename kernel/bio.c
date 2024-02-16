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

#define NBUCKET 13
#define HASH(dev, blockno) (((dev+1) * (blockno+1)) % NBUCKET)

struct bbucket {
  struct spinlock lock;
  struct buf head;
};

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct bbucket buckets[NBUCKET];
} bcache;

extern uint ticks;

static void
install_buf_into_bucket(struct bbucket *bucket, struct buf *buf)
{
  buf->next = bucket->head.next;
  buf->prev = &bucket->head; // ensure the next node of bucket->head is the first node
  bucket->head.next->prev = buf; // place buf at the head of bucket
  bucket->head.next = buf; // set buf as the first node of bucket
}

void
binit(void)
{
  initlock(&bcache.lock, "bcache");

  for (struct buf *b = bcache.buf; b < bcache.buf + NBUF; b++)
    initsleeplock(&b->lock, "buffer");
  for (int i = 0; i < NBUCKET; i++)
  {
    initlock(&bcache.buckets[i].lock, "bucket");
    bcache.buckets[i].head.next = bcache.buckets[i].head.prev = &bcache.buckets[i].head;
  }
  for (int i = 0; i < NBUF; i++)
    install_buf_into_bucket(&bcache.buckets[i%NBUCKET], &bcache.buf[i]);
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  uint index = HASH(dev, blockno);
  struct buf *recylcle = 0;
  uint min_ts = ticks + 1;

  acquire(&bcache.buckets[index].lock);

  // Is the block already cached?
  for(b = bcache.buckets[index].head.next; b != &bcache.buckets[index].head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.buckets[index].lock);
      acquiresleep(&b->lock);
      return b;
    }

    // find a least recent use node to recycle
    if (b->blockno == 0 && b->ts < min_ts)
    {
      min_ts = b->ts;
      recylcle = b;
    }
  }

  // return the node to recycle
  if ((b = recylcle))
  {
    b->dev = dev;
    b->blockno = blockno;
    b->valid = 0;
    b->refcnt = 1;
    release(&bcache.buckets[index].lock);
    acquiresleep(&b->lock);
    return b;
  }
  release(&bcache.buckets[index].lock);

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  acquire(&bcache.lock);
  for (b = bcache.buf; b < bcache.buf+NBUF; b++) {
    if (b->dev == dev && b->blockno == blockno) { // bcache.lock protects dev and blockno
      b->refcnt++;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  struct bbucket *bucket;
  for (int i = 1; i < NBUCKET; i++)
  {
    bucket = &bcache.buckets[(index+i)%NBUCKET];
    min_ts = ticks + 1;
    acquire(&bucket->lock);
    for (b = bucket->head.next; b != &bucket->head; b = b->next)
    {
      if (b->refcnt == 0 && b->ts < min_ts)
      {
        min_ts = b->ts;
        recylcle = b;
      }
    }
    if (recylcle)
      break;
    else
      release(&bucket->lock);
  }
  if (!recylcle)
    panic("bget: no buffers");
  b = recylcle;
  b->next->prev = b->prev;
  b->prev->next = b->next;
  release(&bucket->lock);

  b->dev = dev;
  b->blockno = blockno;
  b->valid = 0;
  b->refcnt = 1;
  release(&bcache.lock);

  acquire(&bcache.buckets[index].lock);
  install_buf_into_bucket(&bcache.buckets[index], b);
  release(&bcache.buckets[index].lock);

  acquiresleep(&b->lock);
  return b;
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
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
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  uint index = HASH(b->dev, b->blockno);
  acquire(&bcache.buckets[index].lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->ts = ticks;
  }
  
  release(&bcache.buckets[index].lock);
}

void
bpin(struct buf *b) {
  uint index = HASH(b->dev, b->blockno);
  acquire(&bcache.buckets[index].lock);
  b->refcnt++;
  release(&bcache.buckets[index].lock);
}

void
bunpin(struct buf *b) {
  uint index = HASH(b->dev, b->blockno);
  acquire(&bcache.buckets[index].lock);
  b->refcnt--;
  release(&bcache.buckets[index].lock);
}


