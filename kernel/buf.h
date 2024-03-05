struct buf {
  int valid;   // has data been read from disk?
  int disk;    // does disk "own" buf?
  uint dev;
  uint blockno;
  struct sleeplock lock;
  uint refcnt;
  uint lastuse; // 用来跟踪最近最后一个使用的块
  struct buf *prev; // LRU cache list
  struct buf *next;
  uchar data[BSIZE];
};

