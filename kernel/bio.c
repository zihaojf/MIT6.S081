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

#define NBUCKETS 13

struct {
  struct spinlock lock;
  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head;
} bcache[NBUCKETS];
struct spinlock lock;
struct buf bufs[NBUF];

void printbucket(int k) {
  printf("printbucket: %d\n",k);
  struct buf *b;
  for(b=bcache[k].head.next;b!=&bcache[k].head;b=b->next) {
    printf("printbucket:%d  %d\n",b->dev,b->blockno);
  }
}

void
binit(void)
{
  // struct buf *b;
  initlock(&lock,"bcache");
  char name[32];

  for(int i = 0;i<NBUCKETS;i++) {
    snprintf(name, sizeof(name), "bcache_bucket_lock_%d", i); // 锁命名
    initlock(&bcache[i].lock, name);
    bcache[i].head.next = &bcache[i].head;
    bcache[i].head.prev = &bcache[i].head;
  }
  printf("bcache!\n");
  for(int i = 0;i<NBUF;i++) {
    struct buf *cache = &bufs[i];
    cache->next = bcache[0].head.next;
    cache->prev = &bcache[0].head;
    initsleeplock(&cache->lock, "buffer");
    bcache[0].head.next->prev = cache;
    bcache[0].head.next = cache;
    cache->t_stamp = ticks;
  }
  // Create linked list of buffers
  // bcache.head.prev = &bcache.head;
  // bcache.head.next = &bcache.head;
  // for(b = bcache.buf; b < bcache.buf+NBUF; b++){
  //   b->next = bcache.head.next;
  //   b->prev = &bcache.head;
  //   initsleeplock(&b->lock, "buffer");
  //   bcache.head.next->prev = b;
  //   bcache.head.next = b;
  // }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  int bucket_id = (dev + 10*blockno) % NBUCKETS;//哈希值
  acquire(&bcache[bucket_id].lock);//获取目标桶锁

  // Is the block already cached?
  for(b = bcache[bucket_id].head.next; b != &bcache[bucket_id].head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache[bucket_id].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache[bucket_id].lock);//释放桶锁

  acquire(&lock);
  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  uint time_cnt = 0xffffffff;
  struct buf *lru = (void*)0;   
  int i = (bucket_id)%NBUCKETS;//哈希值
  int lru_bucket = -1;

  //再次获得目标桶锁
  acquire(&bcache[bucket_id].lock);
  //解锁在上锁之后再次扫描一遍原目标桶
  for(b = bcache[bucket_id].head.next; b != &bcache[bucket_id].head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache[bucket_id].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  for(i = 0;i < NBUCKETS; i++) {
    if(i!=bucket_id)acquire(&bcache[i].lock);//获取其他桶锁
    for(b = bcache[i].head.next; b != &bcache[i].head; b = b->next){
      if(b->refcnt == 0 && b->t_stamp < time_cnt) {//找到空的且时间最短的
        lru = b;
        time_cnt = b->t_stamp;
        lru_bucket = i;
      }
    }
    if(lru) break;
    if(i!=bucket_id) release(&bcache[i].lock); 
  }

  if(!lru) {
    panic("bget: no buffers");
  }

    // int old_bkid = (lru->dev + 10 * lru->blockno) % NBUCKETS;
  // printf("find: oldbucket:%d  newbucket%d   dev:%d   blockno:%d\n",bucket_id,i,dev,blockno);
  if(lru_bucket!=bucket_id) {
    // printf("yes!\n");
      // 移除旧链表
    lru->prev->next = lru->next;
    lru->next->prev = lru->prev;
    // printbucket(lru_bucket);

    //添加新链表
    lru->next = bcache[bucket_id].head.next;
    lru->prev = &bcache[bucket_id].head;
    bcache[bucket_id].head.next->prev = lru;
    bcache[bucket_id].head.next = lru;
    // printbucket(bucket_id);
  }
  lru->dev = dev;
  lru->blockno = blockno;
  lru->valid = 0;
  lru->refcnt = 1;
  lru->t_stamp = ticks;
  
  release(&bcache[lru_bucket].lock);
  if(lru_bucket != bucket_id) release(&bcache[bucket_id].lock);
  release(&lock);
  acquiresleep(&lru->lock);
  
  return lru;
  
  panic("bget: no buffers");
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

  int bucket_id = (b->dev + 10*b->blockno) % NBUCKETS;
  acquire(&bcache[bucket_id].lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    // b->next->prev = b->prev;
    // b->prev->next = b->next;
    // b->next = bcache.head.next;
    // b->prev = &bcache.head;
    // bcache.head.next->prev = b;
    // bcache.head.next = b;
    b->t_stamp = ticks;
  }
  
  release(&bcache[bucket_id].lock);
}

void
bpin(struct buf *b) {
  int bucket_id = (b->dev + 10*b->blockno) % NBUCKETS;
  acquire(&bcache[bucket_id].lock);
  b->refcnt++;
  release(&bcache[bucket_id].lock);
}

void
bunpin(struct buf *b) {
  int bucket_id = (b->dev + 10*b->blockno) % NBUCKETS;
  acquire(&bcache[bucket_id].lock);
  b->refcnt--;
  release(&bcache[bucket_id].lock);
}


