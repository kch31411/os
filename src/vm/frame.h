#include <hash.h>
#include <list.h>

struct page_pointer
{
  struct thread *thread;
  void *addr;

  struct list_elem elem;
};

struct frame
{
  void *phy_addr;
  struct list refer_pages;
  
  struct hash_elem elem;
};

struct hash frames;
static struct hash_iterator frame_iter;

unsigned 
frame_hash (const struct hash_elem *f_, void *aux UNUSED)
{
  const struct frame *f = hash_entry (f_, struct frame, elem);
  return hash_bytes (&f->phy_addr, sizeof f->phy_addr);
}

bool
frame_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED)
{
  const struct frame *a = hash_entry (a_, struct frame, elem);
  const struct frame *b = hash_entry (b_, struct frame, elem);

  return a->phy_addr < b->phy_addr;
}

