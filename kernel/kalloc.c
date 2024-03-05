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
} kmem[NCPU]; // 为每个CPU分配独立的freelist，并用独立的锁进行保护

char *kmen_lock_names[] = {
  "kmen_cpu_0",
  "kmen_cpu_1",
  "kmen_cpu_2",
  "kmen_cpu_3",
  "kmen_cpu_4",
  "kmen_cpu_5",
  "kmen_cpu_6",
  "kmen_cpu_7"
};

void
kinit()
{
  // 初始化所有锁
  for (int i = 0; i < NCPU; i++) {
    initlock(&kmem[i].lock, kmen_lock_names[i]);
  }
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
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  push_off(); // 关闭中断

  int cpu = cpuid();  // 获取运行当前进程的cpu编号

  acquire(&kmem[cpu].lock);
  r->next = kmem[cpu].freelist;
  kmem[cpu].freelist = r;
  release(&kmem[cpu].lock);

  pop_off(); // 打开中断
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  push_off(); // 关闭终端

  int cpu = cpuid();  // 获取运行当前进程的cpu编号

  acquire(&kmem[cpu].lock);

  if (!kmem[cpu].freelist) {  // 当前cpu所持有的freelist为空
    int steal = 64; // 从其他cpu那窃取的页数设置为64页
    for (int i = 0; i < NCPU; i++) {
      if (i == cpu) continue;
      acquire(&kmem[i].lock);
      struct run *r_steal = kmem[i].freelist;
      while (r_steal && steal) {  // 当被窃取的列表不为空且窃取数量仍大于0时
        kmem[i].freelist = r_steal->next;
        r_steal->next = kmem[cpu].freelist; // 插入到头部
        kmem[cpu].freelist = r_steal;
        r_steal = kmem[i].freelist;
        steal--;
      }
      release(&kmem[i].lock);
      if (steal == 0) break;  // 当窃取页数达到了64页就退出
    }
  }

  r = kmem[cpu].freelist;
  if(r)
    kmem[cpu].freelist = r->next;
  release(&kmem[cpu].lock);

  pop_off(); // 打开中断

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
