#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/init.h"

#include "filesys/file.h"
#include "filesys/filesys.h"

#define ARG(x) (*(int*)(f->esp+(4*x)))
#define MAX_CONSOLE_WRITE 400

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
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

bool syscall_create (const char *file, unsigned initial_size)
{
  bool ret = filesys_create (file, initial_size);
  return ret;
}

bool syscall_remove (const char *file)
{
  bool ret = filesys_remove (file);
  return ret;
  // TODO : synchronize all file, filesys function call
}


int syscall_read (int fd, void *buffer, unsigned size)
{
  int ret=-2;

  if ( fd == 0 )
  {
    ret = input_getc (buf, size); 
  }
  else if ( fd == 1 )
  {
    //??? ret = -1;
  }
  else 
  {
    // open fail conditions
    // closed.
    // invalid fd
    //
    // need fd -> filename
    ret = file_read ( thread_current() -> files[fd] , buf, size);
  }

  return ret;
}

int syscall_write (int fd, const void *buffer, unsigned size)
{
  int ret = -2;

  if ( fd == 0 )
  {
    ret = -1; //??
  }
  else if ( fd == 1 )
  {
    // when bigger than few hundreds byte 
    if ( size > MAX_CONSOLE_WRITE )
    {
      ret = putbuf (buf, MAX_CONSOLE_WRITE) 
        +write ( fd, buffer + MAX_CONSOLE_WRITE, size - MAX_CONSOLE_WRITE );
    }
    else 
    {
      ret = putbuf (buf, size);
    }
  }
  else
  {
    // write fail conditions
    // closed
    // other user write
    // invalid fd
    //
    ret = file_write ( thread_current()-> files[fd] , buf, size); 
  }

  return ret;
}

int syscall_open (const char *file)
{
  struct file *open_file;

  open_file = filesys_open (file);
  
  if ( open_file == NULL)
  {
    return -1;
  }
  else 
  {
    struct thread *cur = thread_current();
    if ( list_empty( &cur->empty_fd_list ))
    {
      return cur->fd_idx++;
    }
    else 
    {
      int tmp = -1;
      struct empty_fd *e = list_entry (list_pop_front ( &cur->empty_fd_list ), struct empty_fd, fd_elem) ;
      tmp = e->fd;
      ASSERT( cur->files[tmp] == NULL );
      return tmp;
    }
  }

}

int syscall_filesize (int fd)
{
  int ret=-1;
  ret = file_length ( thread_current() -> files[fd] );
  return ret;
}

void syscall_seek ( int fd, unsigned position)
{
  file_seek ( thread_current() -> files[fd], position);
}

unsigned syscall_tell ( int fd )
{
  unsigned ret = 0;
  ret = file_tell ( thread_current() -> files[fd] );
  return ret;
}

void syscall_close ( int fd )
{
  file_close ( thread_current() -> files[fd] );
  thread_current() -> files[fd] = NULL;
  
  struct empty_fd *e;

  e = palloc_get_page (0);
  ASSERT ( e != NULL);

  e->fd = fd;

  list_push_front ( &thread_current() -> empty_fd_list, &e->fd_elem);
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  int syscall_num = *((int *)(f->esp));

//  printf("\n\n\n\n\n\nsystem call %d\n", syscall_num);
  switch (syscall_num)
  {
    case SYS_HALT: syscall_halt (); break;
    case SYS_EXIT: syscall_exit (ARG(1)); break;
    case SYS_EXEC: f->eax = syscall_exec (ARG(1)); break;
    case SYS_WAIT: f->eax = syscall_wait (ARG(1)); break;
    case SYS_CREATE: f->eax = syscall_create (ARG(1), ARG(2)); break;
    case SYS_REMOVE: f->eax = syscall_remove (ARG(1)); break;
    case SYS_OPEN: f->eax = syscall_open (ARG(1)); break;
    case SYS_FILESIZE: f->eax = syscall_filesize (ARG(1)); break;
    case SYS_READ: f->eax = syscall_read (ARG(1), ARG(2), ARG(3)); break;
    case SYS_WRITE: f->eax = syscall_write (ARG(1), ARG(2), ARG(3)); break;
//    case SYS_SEEK: syscall_seek (ARG(1), ARG(2)); break;
//    case SYS_TELL: f->eax = syscall_tell (ARG(1)); break;
//    case SYS_CLOSE: syscall_close (ARG(1)); break;
    default: ASSERT(false); break;
  } 
}
