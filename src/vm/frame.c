#include "vm/frame.h"
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "userprog/syscall.h"
#include "vm/page.h"

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

  return (a->phy_addr < b->phy_addr);
}

void
frame_init (void)
{
  hash_init (&frames, frame_hash, frame_less, NULL);
  list_init (&frame_list);
  lock_init (&frame_lock);
  isFirst = true;
}

void 
frame_create (void* phy_addr, void* page_addr)
{
  struct frame f;
  struct hash_elem *e;
  struct page_pointer *pp;

  pp = malloc (sizeof (struct page_pointer));
  pp->thread = thread_current ();
  pp->addr = page_addr;

  f.phy_addr = phy_addr;

  if (e = hash_find (&frames, &f.elem) != NULL)
  {
    struct frame *fr = hash_entry (e, struct frame, elem);
    list_push_back (&fr->refer_pages, &pp->elem);
  }

  else
  {
    struct frame *fr = malloc (sizeof (struct frame));
  
    fr->phy_addr = phy_addr;

    list_init (&fr->refer_pages);
    list_push_back (&fr->refer_pages, &pp->elem);

    hash_insert (&frames, &fr->elem);
    list_push_back (&frame_list, &fr->list_elem);
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
  bool isLockAcquired = false;
     if (lock_held_by_current_thread (&file_lock) == false)
    {
      lock_acquire (&file_lock);
      isLockAcquired = true;
    }

  struct frame f;
  struct frame *fr;
  struct hash_elem *eh;
  struct list *l;
  struct list_elem *el;

  f.phy_addr = phy_addr;

  eh = hash_find (&frames, &f.elem);

  ASSERT (eh != NULL);

  fr = hash_entry (eh, struct frame, elem);
  l = &fr->refer_pages;
  
  if (isForce == true)
  {
    while (list_empty (l) == false)
    {
      struct page_pointer *pp = list_entry (list_pop_front (l), struct page_pointer, elem);
      free (pp);
    }
  }

  else
  { 
    for (el = list_begin (l); el != list_end (l); el = list_next (l))
    {
      struct page_pointer *pp = list_entry (el, struct page_pointer, elem);

      if (pp->thread == thread_current ()) 
      {
        list_remove (el);

        free (pp);
        break;
      }
    }
  }
  
  if (isForce == true || list_size (l) == 0) 
  {
    hash_delete (&frames, eh);
    list_remove (&fr->list_elem);
    free (fr);
  }

    if (lock_held_by_current_thread (&file_lock) && isLockAcquired == true) lock_release (&file_lock);
}

bool
frame_is_accessed (struct frame *f)
{
  struct list_elem *e;
  bool ret = false;

  for (e = list_begin (&f->refer_pages); e != list_end (&f->refer_pages); e = list_next (e))
  {
    struct page_pointer *pp = list_entry (e, struct page_pointer, elem);
    ASSERT (pp != NULL);
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
frame_victim ()
{
  struct hash_elem *e;
  
  ASSERT ( list_empty (&frame_list) == false);

  while (1)
  {
    struct list_elem *e = list_pop_front (&frame_list);
    struct frame *f = list_entry (e, struct frame, list_elem);

    struct page_pointer *pp = list_entry (list_front (&f->refer_pages), struct page_pointer, elem);
    struct page *p = page_lookup (pp->thread, pp->addr);
    
    list_push_back (&frame_list, e);

    if (frame_is_accessed (f) == false && p->fromDisk == false )
    {
       return f;
    }

    else frame_reset_accessed (f);
  }
}
