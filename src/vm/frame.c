#include "frame.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.c"

unsigned 
frame_hash (const struct hash_elem *f_, void *aux UNUSED)
{
  const struct frame *f = hash_entry (f_, struct frame, elem);
  return hash_bytes (&f->phy_addr, sizeof f->phy_addr);
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

void 
frame_delete (void *phy_addr, bool isForce)
{
  struct frame f;
  struct frame *fr;
  struct hash_elem *e;
  struct thread *t = thread_current ();
  struct list l;
  struct list_elem *e;

  f.phy_addr = phy_addr;

  e = hash_find (&frames, &f.elem);
  fr = hash_entry (e, struct frame, elem);

  l = fr->refer_pages;
  for (e = list_begin (&l); e != list_end (&l); e = list_next (e))
  {
    struct page_pointer *pp = list_entry (e, struct page_pointer, elem);

    if (isForce == true)
    {
      free (pp);
    }

    else if (pp->thread == t) 
    {
      list_remove (e);
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
  for (; ; hash_next (&frame_iter))
  {
    if (frame_iter == NULL) frame_iter = hash_first (&frame_iter, &frames);

    struct frame *f = hash_entry (hash_cur (&frame_iter), struct frame, elem);

    if (frame_is_accessed (f) == false) return f;
    else frame_reset_accessed (f);
  } 
}
