#include <hash.h>
#include <stdbool.h>
#include <debug.h>
#include "devices/disk.h"
#include "threads/thread.h"

struct page  // supplement page table
{
  struct hash_elem elem;   // for hash
  void *addr;                   // virtual address
  disk_sector_t disk_no;
  bool isDisk;  
};

unsigned page_hash (const struct hash_elem *p_, void *aux UNUSED);
bool page_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED);
void page_create (void *addr);
struct page *page_lookup (struct thread *t, const void *addr);
void page_delete (void *addr);

