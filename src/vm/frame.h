#include <hash.h>
#include <list.h>
#include <debug.h>
#include "threads/thread.h"

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

unsigned frame_hash (const struct hash_elem *f_, void *aux UNUSED);
bool frame_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED);
void frame_create (void* phy_addr, void* page_addr);
struct frame* frame_find (void *phy_addr);
void frame_delete (void *phy_addr, bool isForce);
bool frame_is_accessed (struct frame *f);
void frame_reset_accessed (struct frame *f);
struct frame* victim (void);

