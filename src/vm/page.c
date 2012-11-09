#include "vm/page.h"

/* Returns a hash value for page p. */
unsigned
page_hash (const struct hash_elem *p_, void *aux UNUSED)
{
  const struct page *p = hash_entry (p_, struct page, elem);
  return hash_bytes (&p->addr, sizeof p->addr);
}

/* Returns true if page a precedes page b. */
bool
page_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED)
{
  const struct page *a = hash_entry (a_, struct page, elem);
  const struct page *b = hash_entry (b_, struct page, elem);

  return a->addr < b->addr;
}

void 
page_create (void *addr)
{
  struct page p;
  struct hash_elem *e;
  struct thread *t = thread_current ();
  struct page *new;

  p.addr = addr;
  ASSERT (e = hash_find (&t->pages, &p.elem) == NULL);

  new = malloc (sizeof (struct page));

  new->addr = addr;
  new->disk_no = NULL;
  new->isDisk = false; 

  hash_insert (&t->pages, &new->elem);
}

/* Returns the page containing the given virtual address,
 *    or a null pointer if no such page exists. */
struct page *
page_lookup (struct thread *t, const void *addr)
{
  struct page p;
  struct hash_elem *e;

  p.addr = addr;
  e = hash_find (&t->pages, &p.elem);
  return e != NULL ? hash_entry (e, struct page, elem) : NULL;
}

void 
page_delete (void *addr)
{
  struct page p;
  struct hash_elem *e;
  struct thread *t = thread_current ();
  struct page *del;

  p.addr = addr;
  e = hash_delete (&t->pages, &p.elem);

  ASSERT (e != NULL);

  del = hash_entry (e, struct page, elem);
  free (del);
}
