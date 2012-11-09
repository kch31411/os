#include "vm/frame.h"
#include "threads/vaddr.h"
#include "threads/thread.h"

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

void
frame_init (void)
{
  hash_init (&frames, frame_hash, frame_less, NULL);
  hash_first (&frame_iter, &frames);
}

void 
frame_create (void* phy_addr, void* page_addr)
{
  struct frame f;
  struct hash_elem *e;
  struct page_pointer *pp;
  struct thread *t = thread_current ();

  pp = malloc (sizeof (struct page_pointer));
  pp->thread = t;
  pp->addr = page_addr;

  f.phy_addr = phy_addr;

  if (e = hash_find (&frames, &f.elem) != NULL)
  {
    struct frame *fr = hash_entry (e, struct frame, elem);
    list_insert (&fr->refer_pages, &pp->elem);
  }

  else
  {
    struct frame *fr = malloc (sizeof (struct frame));
  
    fr->phy_addr = phy_addr;
    list_init (&fr->refer_pages);
    list_insert (&fr->refer_pages, &pp->elem);
    
    hash_insert (&frames, &fr->elem);
  }
}

struct frame*
frame_find (void *phy_addr)
{
  struct frame f;
  struct hash_elem *e;

  f.phy_addr = phy_addr;
  e = hash_find (&frames, &f.elem);
  return e != NULL ? hash_entry (e, struct frame, elem) : NULL;
}

void 
frame_delete (void *phy_addr, bool isForce)
{
  struct frame f;
  struct frame *fr;
  struct hash_elem *eh;
  struct thread *t = thread_current ();
  struct list l;
  struct list_elem *el;

  f.phy_addr = phy_addr;

  eh = hash_find (&frames, &f.elem);
  fr = hash_entry (eh, struct frame, elem);

  l = fr->refer_pages;
  for (el = list_begin (&l); el != list_end (&l); el = list_next (el))
  {
    struct page_pointer *pp = list_entry (el, struct page_pointer, elem);

    if (isForce == true)
    {
      free (pp);
    }

    else if (pp->thread == t) 
    {
      list_remove (el);
      free (pp);
      break;
    }
  }

  if (isForce == true || list_size (&l) == 0) free (fr);
}

bool
frame_is_accessed (struct frame *f)
{
  struct list_elem *e;
  bool ret = false;

  for (e = list_begin (&f->refer_pages); e != list_end (&f->refer_pages); e = list_next (e))
  {
    struct page_pointer *pp = list_entry (e, struct page_pointer, elem);

    ret |= pagedir_is_accessed (pp->thread->pagedir, pp->addr);
  }

  return ret;
}

void 
frame_reset_accessed (struct frame *f)
{
  struct list_elem *e;

  for (e = list_begin (&f->refer_pages); e != list_end (&f->refer_pages); e = list_next (e))
  {
    struct page_pointer *pp = list_entry (e, struct page_pointer, elem);

    pagedir_set_accessed (pp->thread->pagedir, pp->addr, false);
  }
}

struct frame* 
victim ()
{
  struct hash_elem *e;

  while (e = hash_next (&frame_iter))
  {
    if (e == NULL) 
    {
      hash_first (&frame_iter, &frames);
      continue;
    }

    struct frame *f = hash_entry (e, struct frame, elem);

    if (frame_is_accessed (f) == false) return f;
    else frame_reset_accessed (f);
  } 
}
