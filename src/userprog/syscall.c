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

#define ARG(x) (f->esp+(4*x))
#define MAX_CONSOLE_WRITE 400

static void syscall_handler (struct intr_frame *);

struct lock file_lock;

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
  
  if (thread_current ()->files[fd] == NULL) 
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
//    syscall_exit (-1);
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
      ret = file_read (thread_current()->files[fd], buffer, size);
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
      ret = file_write (thread_current()->files[fd], buffer, size); 
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
      cur->files[cur->fd_idx] = open_file;
      ret = cur->fd_idx++;
    }

    else 
    {
      struct empty_fd *e = list_entry (list_pop_front (&cur->empty_fd_list), struct empty_fd, fd_elem);
      ret = e->fd;

      palloc_free_page (e); 

      ASSERT (cur->files[ret] == NULL);
      cur->files[ret] = open_file;
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
    ret = file_length (thread_current()->files[fd]);
    lock_release (&file_lock);
  }

  return ret;
}

void syscall_seek (int fd, unsigned position)
{
  if (is_valid_file (fd) == false) return;

  lock_acquire (&file_lock);
  file_seek (thread_current()->files[fd], position);
  lock_release (&file_lock);
}

unsigned syscall_tell (int fd)
{
  unsigned ret;

  if (is_valid_file (fd) == false) ret = 0;

  else
  {
    lock_acquire (&file_lock);
    ret = file_tell (thread_current()->files[fd]);
    lock_release (&file_lock);
  }

  return ret;
}

void syscall_close (int fd)
{
  struct empty_fd *e = palloc_get_page (0);
  ASSERT (e != NULL);

  if (is_valid_file (fd) == false) return;

  lock_acquire (&file_lock);
  file_close (thread_current ()->files[fd]);
  lock_release (&file_lock);

  thread_current()->files[fd] = NULL;

  e->fd = fd;

  list_push_front (&thread_current ()->empty_fd_list, &e->fd_elem);
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
    default: ASSERT(false); break;
  } 
}
