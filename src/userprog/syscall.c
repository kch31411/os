#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/init.h"

#include "userprog/process.h"
#include "devices/input.h"
#include "filesys/filesys.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"

#include "vm/frame.h"
#include "vm/page.h"


#define ARG(x) (f->esp+(4*x))
#define MAX_CONSOLE_WRITE 400

static void syscall_handler (struct intr_frame *);


bool is_valid_address (void *a)
{
  return (is_user_vaddr(a) && a >= 0);
}

bool is_valid_file (int fd)
{
  if (fd < 2 || fd >= thread_current ()->fd_idx) 
  {
    return false;
  }
  
  if (thread_current ()->files[fd]->file == NULL) 
  {
    return false;
  }

  return true;
}

int arg_get (int *p)
{
  if (is_valid_address (p) == true) 
  {
    return *p;
  }
  
  else 
  {
    syscall_exit (-1);
    return -1;  
  }
}

void syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&file_lock);
}

void syscall_halt (void)
{
  power_off();
}

void syscall_exit (int status)
{
  //printf ("PID: %d\n", thread_current ()->tid);
  printf ("%s: exit(%d)\n", thread_current ()->name, status);
  thread_current ()->exit_status = status;

  thread_exit();
}

int syscall_exec (const char *cmd_line)
{
  return process_execute (cmd_line);
}

int syscall_wait (int pid)
{
  return process_wait (pid);
}

bool syscall_create (const char *file, unsigned int initial_size)
{
  if (file == NULL) 
  {
    syscall_exit (-1);
  }

  lock_acquire (&file_lock);
  bool ret = filesys_create(file, initial_size);
  lock_release (&file_lock);
  
  return ret;
}

bool syscall_remove (const char *file)
{
  if (file == NULL) 
  {
    syscall_exit (-1);
  }

  lock_acquire (&file_lock);
  bool ret = filesys_remove (file);
  lock_release (&file_lock);
 
  return ret;
}

int syscall_read (int fd, void *buffer, unsigned size)
{
  int ret = -2;
  unsigned i;

  if (fd == 0)
  {
    for (i = 0; i < size; i++)
    {
      *(uint8_t *)(buffer++) = input_getc();
    }

    ret = size;
  }

  else if (fd == 1)
  {
    ret = -1;
  }

  else 
  {
    if (is_valid_file (fd) == false) ret = -1;
    
    else 
    {
      lock_acquire (&file_lock);
      ret = file_read (thread_current()->files[fd]->file, buffer, size);
      lock_release (&file_lock);
    }
  }

  return ret;
}

int syscall_write (int fd, const void *buffer, unsigned size)
{
  int ret = -2;
  unsigned i;

  if (fd == 0)
  {
    ret = -1;
  }

  else if (fd == 1)
  {
    for (i = 0; i < size / MAX_CONSOLE_WRITE; i++)
    {
      putbuf (buffer, MAX_CONSOLE_WRITE);
    }
    putbuf (buffer, size % MAX_CONSOLE_WRITE);
  }

  else
  {
    if (is_valid_file (fd) == false) ret = -1;

    else
    {
      lock_acquire (&file_lock);
      ret = file_write (thread_current()->files[fd]->file, buffer, size); 
      lock_release (&file_lock);
    }
  }

  return ret;
}

int syscall_open (const char *file)
{
  int ret;

  if (file == NULL)
  {
    syscall_exit(-1);
  }

  lock_acquire (&file_lock);
  struct file *open_file = filesys_open (file);
  lock_release (&file_lock);
  
  if (open_file == NULL)
  {
    return -1;
  }

  else 
  {
    struct thread *cur = thread_current();
    
    if (list_empty (&cur->empty_fd_list) == true)
    {
      cur->files[cur->fd_idx]= palloc_get_page(0);
      cur->files[cur->fd_idx]->file = open_file;
      cur->files[cur->fd_idx]->is_mapped = false;
      ret = cur->fd_idx++;
    }

    else 
    {
      struct empty_fd *e = list_entry (list_pop_front (&cur->empty_fd_list), struct empty_fd, fd_elem);
      ret = e->fd;

      palloc_free_page (e); 

      cur->files[ret] = palloc_get_page(0);
      cur->files[ret]->file = open_file;
      cur->files[ret]->is_mapped = false;
    }
  }

  return ret;
}

int syscall_filesize (int fd)
{
  int ret;

  if (is_valid_file (fd) == false) ret = -1;
  
  else
  {
    lock_acquire (&file_lock);
    ret = file_length (thread_current()->files[fd]->file);
    lock_release (&file_lock);
  }

  return ret;
}

void syscall_seek (int fd, unsigned position)
{
  if (is_valid_file (fd) == false) return;

  lock_acquire (&file_lock);
  file_seek (thread_current()->files[fd]->file, position);
  lock_release (&file_lock);
}

unsigned syscall_tell (int fd)
{
  unsigned ret;

  if (is_valid_file (fd) == false) ret = 0;

  else
  {
    lock_acquire (&file_lock);
    ret = file_tell (thread_current()->files[fd]->file);
    lock_release (&file_lock);
  }

  return ret;
}

void syscall_close (int fd)
{
  struct empty_fd *e = palloc_get_page (0);
  struct thread *t = thread_current();
  ASSERT (e != NULL);

  if (is_valid_file (fd) == false) return;

  bool isLockAcquired = false;
  if (lock_held_by_current_thread (&file_lock) == false) 
  {
    lock_acquire (&file_lock);
    isLockAcquired = true;
  }

  if ( t->files[fd]->is_mapped )
  {
    syscall_munmap ( t->files[fd]->mapid );
  }

  file_close (t->files[fd]->file);
  if ( isLockAcquired == true ) lock_release (&file_lock);

  t->files[fd]->file = NULL;

  e->fd = fd;
  list_push_front (&t->empty_fd_list, &e->fd_elem);

  palloc_free_page(t->files[fd]);
  t->files[fd] = NULL;
}

int syscall_mmap (int fd, void *addr, struct intr_frame *f)
{
  if (is_valid_file(fd) == false) return -1;
  if ( pg_ofs (addr) != 0 || is_valid_address(addr) == false || 
      addr > f->esp || addr < 0x08048000) return -1;
  // addr in code, stack??? page already exist.
  // anyway can save the start position of code segment when load is done 
  
  struct thread *t = thread_current();
  struct file *file = t->files[fd]->file;

  int size = syscall_filesize(fd);
  
  int page_cnt = size / PGSIZE;
  if (size % PGSIZE != 0) page_cnt += 1;

  lock_acquire(&file_lock);
  off_t pos = file_tell (file);
  lock_release(&file_lock);

  int i;
  void *cur_addr = addr;
  for (i=0; i<page_cnt; i++)
  {
    if (pagedir_get_page(t->pagedir, cur_addr) != NULL) return -1;
    if (page_lookup(t, cur_addr) != NULL ) return -1;
      // XXX : is it enought???

    cur_addr += PGSIZE;
  }

  cur_addr = addr;
  int tmp_size = size;
  int tmp_pos = pos;
  for (i=0; i<page_cnt; i++)
  {
    struct page *new = page_create_return (cur_addr);
    new->fromDisk = true;
    new->file = file;
    new->file_start = tmp_pos;

    if (tmp_size >= PGSIZE)
    {
      tmp_size -= PGSIZE;
      tmp_pos += PGSIZE;
      new->file_size = PGSIZE;
    }
    else 
    {
      new->file_size = tmp_size;
    }

    cur_addr += PGSIZE;
  }

  /*
  // no overlapping in user virtual addr,
  // now get frames

  // XXX : synch problem
  void *kpage = palloc_get_multiple (PAL_USER | PAL_ZERO, page_cnt);
  lock_acquire(&file_lock);
  int written = file_read ( file, kpage, size);
  lock_release(&file_lock);
  ASSERT (size == written);

//  printf("upage : %x,  kpage : %x\n", addr, kpage);

  cur_addr = addr;
  void *cur_kpage = kpage;
  for(i=0; i<page_cnt; i++)
  {
    // XXX: sync problem?
    pagedir_set_page ( t->pagedir, cur_addr, cur_kpage, true);
    page_create (cur_addr);

    cur_addr += PGSIZE;
    cur_kpage += PGSIZE;
  }
*/

  int ret = -2;
  if (list_empty (&t->empty_mmap_list) == true)
  {
    t->mmap_list[t->mmap_idx] = fd;
    ret = t->mmap_idx++;
  }
  else
  {
    struct empty_mmap *e = list_entry (list_pop_front (&t->empty_mmap_list), struct empty_mmap, mmap_elem);
    ret = e->mapid;
    
    palloc_free_page(e);

    t->mmap_list[ret] = fd;
  }

  lock_acquire(&file_lock);
  file_seek (file, pos);
  lock_release(&file_lock);


  t->files[fd]->is_mapped = true;
  t->files[fd]->mapid = ret;
  t->files[fd]->mm_size = size; // XXX: file size can varies hmm...
  t->files[fd]->mm_addr = addr;

  return ret;
}
void syscall_munmap (int mapid)
{
  struct thread *t = thread_current();
  int fd = t->mmap_list[mapid];
  struct file_info *file_info = t->files[fd];
  int size = file_info->mm_size;
  void *addr = file_info->mm_addr;
  void *kpage;

  bool isLockAcquired = false;
  if (lock_held_by_current_thread (&file_lock) == false) 
  {
    lock_acquire (&file_lock);
    isLockAcquired = true;
  }
  off_t pos = file_tell(file_info->file);

  int idx = 0;
  while (size > 0)
  {
    struct page *p = page_lookup(t, addr);
    
    /* now no such case
    if (p->isDisk == true)
    {
      bool success = false;

      kpage = palloc_get_page (PAL_USER | PAL_ZERO);

      lock_acquire (&frame_lock);

      if (kpage != NULL)
      { 
        success = pagedir_set_page (t->pagedir, pg_round_down (addr), kpage, true);
      } 
      ASSERT (success == true);

      swap_in (p->disk_no, kpage);

      p->isDisk = false;
      p->disk_no = NULL;

      lock_release (&frame_lock);
    }
    else
    {
      kpage = pagedir_get_page(t->pagedir, addr);
    }
    */

    kpage = pagedir_get_page(t->pagedir, addr);
    if (kpage == NULL)
    {
      page_delete (pg_round_down (addr));
    }
    else 
    {
      if (pagedir_is_dirty(t->pagedir, addr))
      {
        // XXX is dirty bit reliable after swapping???
        int written = file_write_at (file_info->file, kpage, size%PGSIZE, idx);
        ASSERT(written == size%PGSIZE);
      }

      pagedir_clear_page(t->pagedir, pg_round_down (addr));
      page_delete (pg_round_down (addr));

      // XXX: can always delete???
      frame_delete (kpage, true);
    }

    size -= size;
    idx = idx + PGSIZE;
    addr += PGSIZE;
  }

  file_seek( file_info->file, pos);
  if ( isLockAcquired == true ) lock_release (&file_lock);

  t->mmap_list[mapid] = 0;
  file_info->is_mapped = false;

  struct empty_mmap *e2 = palloc_get_page(0);
  // XXX : maybe malloc is better than palloc

  e2->mapid = t->files[fd]->mapid;
  list_push_front (&t->empty_mmap_list, &e2->mmap_elem);
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  int syscall_num = arg_get(ARG(0));

  switch (syscall_num)
  {
    case SYS_HALT: syscall_halt (); break;
    case SYS_EXIT: syscall_exit (arg_get(ARG(1))); break;
    case SYS_EXEC: f->eax = syscall_exec (arg_get(ARG(1))); break;
    case SYS_WAIT: f->eax = syscall_wait (arg_get(ARG(1))); break;
    case SYS_CREATE: f->eax = syscall_create (arg_get(ARG(1)), arg_get(ARG(2))); break;
    case SYS_REMOVE: f->eax = syscall_remove (arg_get(ARG(1))); break;
    case SYS_OPEN: f->eax = syscall_open (arg_get(ARG(1))); break;
    case SYS_FILESIZE: f->eax = syscall_filesize (arg_get(ARG(1))); break;
    case SYS_READ: f->eax = syscall_read (arg_get(ARG(1)), arg_get(ARG(2)), arg_get(ARG(3))); break;
    case SYS_WRITE: f->eax = syscall_write (arg_get(ARG(1)), arg_get(ARG(2)), arg_get(ARG(3))); break;
    case SYS_SEEK: syscall_seek (arg_get(ARG(1)), arg_get(ARG(2))); break;
    case SYS_TELL: f->eax = syscall_tell (arg_get(ARG(1))); break;
    case SYS_CLOSE: syscall_close (arg_get(ARG(1))); break;
    case SYS_MMAP: f->eax = syscall_mmap (arg_get(ARG(1)), arg_get(ARG(2)), f); break;
    case SYS_MUNMAP: syscall_munmap (arg_get(ARG(1))); break;
    default: ASSERT(false); break;
  } 
}
