#include <hash.h>

struct page { // supplement page table
  struct hash_elem hash_elem;   // for hash
  void *addr;                   // virtual address
  // XXX addr of swap slot
  bool freeable;                   // when process terminate 
  // XXX add more
};


// XXX struct pgae* page_create
unsigned page_hash (const struct hash_elem *p_, void *aux);
bool page_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux);
struct page * page_lookup (const void *address);
