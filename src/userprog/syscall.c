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
#define MIN(a, b) (((a) < (b))? (a): (b))

static void syscall_handler (struct intr_frame *);

bool is_valid_address (void *a)
{
  return (is_user_vaddr(a) && a >= 0);
}

bool is_valid_file (int fd)
{
  if (fd < 2 || fd >= thread_current ()->fd_idx || thread_current ()->files[fd] == NULL) 
  {
    return false;
  }
  
  if (thread_current ()->files[fd]->file == NULL) 
  {
    return false;
  }
  if (thread_current ()->files[fd]->is_closed == true)
  {
    return false;
  }
  if (thread_current ()->files[fd]->is_dir == true)
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

  bool isLockAcquired = false;
  if (lock_held_by_current_thread (&file_lock) == false)
  {
    lock_acquire (&file_lock);
    isLockAcquired = true;
  }
  bool ret = filesys_create(file, initial_size);
  if (lock_held_by_current_thread (&file_lock) && isLockAcquired == true) lock_release (&file_lock);
 
  return ret;
}

bool syscall_remove (const char *file)
{
  if (file == NULL) 
  {
    syscall_exit (-1);
  }
  
  bool isLockAcquired = false;
  if (lock_held_by_current_thread (&file_lock) == false)
  {
    lock_acquire (&file_lock);
    isLockAcquired =true;
  }
  bool ret = filesys_remove (file);
  if (lock_held_by_current_thread (&file_lock) && isLockAcquired == true) lock_release (&file_lock);

  return ret;
}

int syscall_read (int fd, void *buffer, unsigned size)
{
  int ret = -2;
  unsigned i;

  bool isLockAcquired = false;
  if (lock_held_by_current_thread (&file_lock) == false)
  {
    lock_acquire (&file_lock);
    isLockAcquired = true;
  }

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

  else if (is_valid_file (fd) == false) 
  {
    ret = -1;
  }

  else 
  {
    ret = file_read (thread_current()->files[fd]->file, buffer, size);
  }
  
  if (lock_held_by_current_thread (&file_lock) && isLockAcquired == true) lock_release (&file_lock);

//  printf("read : %d size%d\n", ret, syscall_filesize (fd));
  return ret;
}

int syscall_write (int fd, const void *buffer, unsigned size)
{
  int ret = -2;
  unsigned i;

  bool isLockAcquired = false;
  if (lock_held_by_current_thread (&file_lock) == false)
  {
    lock_acquire (&file_lock);
    isLockAcquired = true;
  }

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
      ret = file_write (thread_current()->files[fd]->file, buffer, size); 
    }
  }

  if (lock_held_by_current_thread (&file_lock) && isLockAcquired == true) lock_release (&file_lock);
  
  return ret;
}

int syscall_open (const char *file)
{
  int ret;

  if (file == NULL)
  {
    syscall_exit(-1);
  }

  bool isLockAcquired = false;
  if (lock_held_by_current_thread (&file_lock) == false)
  {
    lock_acquire (&file_lock);
    isLockAcquired = true;
  }

  bool is_dir = false;
  void *open_file = filesys_open (file, &is_dir);

  if (open_file == NULL)
  {
    //printf("open file is null\n");
    if (lock_held_by_current_thread (&file_lock) && isLockAcquired == true) lock_release (&file_lock);
    return -1;
  }

  else 
  {
    struct thread *cur = thread_current();
    
    if (list_empty (&cur->empty_fd_list) == true) ret = cur->fd_idx++;

    else 
    {
      struct empty_fd *e = list_entry (list_pop_front (&cur->empty_fd_list), struct empty_fd, fd_elem);
      ret = e->fd;

      palloc_free_page (e); 
    }  

    cur->files[ret]= palloc_get_page(0);
    cur->files[ret]->file = open_file;
    cur->files[ret]->is_dir = is_dir;
    cur->files[ret]->is_mapped = false;
    cur->files[ret]->is_closed = false;
  }

  if (lock_held_by_current_thread (&file_lock) && isLockAcquired == true) lock_release (&file_lock);
 
  return ret;
}

int syscall_filesize (int fd)
{
  int ret;

  if (is_valid_file (fd) == false) ret = -1;
  
  else
  {
    bool isLockAcquired = false;
    if (lock_held_by_current_thread (&file_lock) == false)
    {
      lock_acquire (&file_lock);
      isLockAcquired = true;
    }    
    
    ret = file_length (thread_current()->files[fd]->file);
    
    if (lock_held_by_current_thread (&file_lock) && isLockAcquired == true) lock_release (&file_lock);
  }

  return ret;
}

void syscall_seek (int fd, unsigned position)
{
  if (is_valid_file (fd) == false) return;
  
  bool isLockAcquired = false;
  if (lock_held_by_current_thread (&file_lock) == false)
  {
    lock_acquire (&file_lock);
    isLockAcquired = true;
  }  
  
  file_seek (thread_current()->files[fd]->file, position);
  
  if (lock_held_by_current_thread (&file_lock) && isLockAcquired == true) lock_release (&file_lock);
}

unsigned syscall_tell (int fd)
{
  unsigned ret;

  if (is_valid_file (fd) == false) ret = 0;

  else
  { 
    if (is_valid_file (fd) == false) return;
    bool isLockAcquired = false;
    if (lock_held_by_current_thread (&file_lock) == false)
    {
      lock_acquire (&file_lock);
      isLockAcquired = true;
    }  

    ret = file_tell (thread_current()->files[fd]->file);
    
    if (lock_held_by_current_thread (&file_lock) && isLockAcquired == true) lock_release (&file_lock);
  }

  return ret;
}

void syscall_close (int fd)
{
  struct empty_fd *e = palloc_get_page (0);
  struct thread *t = thread_current();
  ASSERT (e != NULL);

  if (fd < 2 || fd >= thread_current ()->fd_idx) 
  {
    return ;
  }
  
  if (thread_current ()->files[fd]->file == NULL) 
  {
    return ;
  }
  if (thread_current ()->files[fd]->is_closed == true)
  {
    return ;
  }

//  if (is_valid_file (fd) == false) return;

  bool isLockAcquired = false;
  if (lock_held_by_current_thread (&file_lock) == false) 
  {
    lock_acquire (&file_lock);
    isLockAcquired = true;
  }

  if (t->files[fd]->is_mapped == false) 
  {
    file_close (t->files[fd]->file);

    e->fd = fd;
    list_push_front (&t->empty_fd_list, &e->fd_elem);

    palloc_free_page (t->files[fd]);
    t->files[fd] = NULL;
  }

  else 
  {
    t->files[fd]->is_closed = true;
  }
    
  if (isLockAcquired == true) lock_release (&file_lock);
}

int syscall_mmap (int fd, void *addr)
{
  if (is_valid_file (fd) == false) return -1;

  if (pg_ofs (addr) != 0 || is_valid_address(addr) == false || addr < 0x08048000) return -1;

  bool isLockAcquired = false;
  if (lock_held_by_current_thread (&file_lock) == false) 
  {
    lock_acquire (&file_lock);
    isLockAcquired = true;
  }

  struct thread *t = thread_current();
  struct file *file = t->files[fd]->file;

  int size = syscall_filesize(fd);
  
  int page_cnt = size / PGSIZE;
  if (size % PGSIZE != 0) page_cnt += 1;

  off_t pos = file_tell (file);

  int i;
  void *cur_addr = addr;
  for (i = 0; i < page_cnt; i++)
  {
    if (pagedir_get_page (t->pagedir, cur_addr) != NULL || page_lookup (t, cur_addr) != NULL)
    {
      if (isLockAcquired == true) lock_release (&file_lock);
      return -1;
    }

    cur_addr += PGSIZE;
  }

  cur_addr = addr;
  int tmp_size = size;
  int tmp_pos = pos;
  for (i = 0; i < page_cnt; i++)
  {
    struct page *new;
    page_create (cur_addr);
    new = page_lookup (t, cur_addr);

    new->fromDisk = true;
    new->file = file;
    new->file_start = tmp_pos;
    new->writable = true;

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
  
  int ret = -1;
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

  file_seek (file, pos);

  t->files[fd]->is_mapped = true;
  t->files[fd]->mapid = ret;
  t->files[fd]->mm_size = size; 
  t->files[fd]->mm_addr = addr;

  if (isLockAcquired == true) lock_release (&file_lock);
  
  return ret;
}

void syscall_munmap (int mapid)
{
  struct thread *t = thread_current ();
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

  int idx = 0;
  while (size > 0)
  {
    struct page *p = page_lookup(t, addr);

    kpage = pagedir_get_page(t->pagedir, addr);
    if (kpage == NULL)
    {
      page_delete (pg_round_down (addr));
    }

    else 
    {
      if (pagedir_is_dirty(t->pagedir, addr))
      {
        int written = file_write_at (file_info->file, kpage, MIN(size, PGSIZE), idx);
        ASSERT(written == MIN(size, PGSIZE));
      }

      pagedir_clear_page(t->pagedir, pg_round_down (addr));
      page_delete (pg_round_down (addr));

      frame_delete (kpage, true);
    }

    size -= PGSIZE;
    idx = idx + PGSIZE;
    addr += PGSIZE;
  }

  t->mmap_list[mapid] = 0;
  file_info->is_mapped = false;

  if (file_info->is_closed == true)
  {
    syscall_close (fd);
  }

  struct empty_mmap *em = palloc_get_page(0);

  em->mapid = t->files[fd]->mapid;
  list_push_front (&t->empty_mmap_list, &em->mmap_elem);

  if (isLockAcquired == true) lock_release (&file_lock);
}

// should implement lock
bool syscall_chdir (const char *dir)
{
  return filesys_chdir (dir); 
}

bool syscall_mkdir (const char *dir)
{
  return filesys_mkdir (dir);
}

bool syscall_readdir (int fd, char *name)
{
  if (syscall_isdir (fd) == false) return false;

  else  
  {
    return filesys_readdir (thread_current ()->files[fd]->file, name);
  }
}

bool syscall_isdir (int fd)
{
  return (thread_current ()->files[fd]->is_dir == true);
}

int syscall_inumber (int fd)
{
//  if (syscall_isdir (fd) == true)
  if (thread_current ()->files[fd]->is_dir)
  {
    return inode_get_inumber (dir_get_inode (thread_current ()->files[fd]->file));
  }

  else
  {
    return inode_get_inumber (file_get_inode (thread_current ()->files[fd]->file));
  }
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  if ( is_user_vaddr(f->esp)) thread_current()->esp = f->esp;
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
    case SYS_MMAP: f->eax = syscall_mmap (arg_get(ARG(1)), arg_get(ARG(2))); break;
    case SYS_MUNMAP: syscall_munmap (arg_get(ARG(1))); break;
    case SYS_CHDIR: f->eax = syscall_chdir (arg_get(ARG(1))); break;
    case SYS_MKDIR: f->eax =  syscall_mkdir (arg_get(ARG(1))); break;
    case SYS_READDIR: f->eax = syscall_readdir (arg_get(ARG(1)), arg_get(ARG(2))); break;
    case SYS_ISDIR: f->eax = syscall_isdir (arg_get(ARG(1))); break;
    case SYS_INUMBER: f->eax = syscall_inumber (arg_get(ARG(1))); break;
    default: ASSERT(false); break;
  } 
}
