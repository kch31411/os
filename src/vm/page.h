#include <hash.h>

struct page { // supplement page table
  struct hash_elem hash_elem;   // for hash
  void *addr;                   // virtual address
  // XXX addr of swap slot
  bool canFree;                   // when process terminate 
  // XXX add more
};


// XXX struct pgae* page_create


/* Returns a hash value for page p. */
unsigned
page_hash (const struct hash_elem *p_, void *aux UNUSED)
{
  const struct page *p = hash_entry (p_, struct page, hash_elem);
  return hash_bytes (&p->addr, sizeof p->addr);
}

/* Returns true if page a precedes page b. */
bool
page_less (const struct hash_elem *a_, const struct hash_elem *b_,
               void *aux UNUSED)
{
  const struct page *a = hash_entry (a_, struct page, hash_elem);
  const struct page *b = hash_entry (b_, struct page, hash_elem);

  return a->addr < b->addr;
}


struct page * page_lookup (const void *address);

