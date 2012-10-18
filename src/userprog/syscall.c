#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/init.h"

#define ARG(x) (*(int*)(f->esp-(4*x)))

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

void syscall_open (const char *file)
{

}

void syscall_filesize (int fd)
{

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
//    case SYS_CREATE: f->eax = syscall_create (ARG(1), ARG(2)); break;
//    case SYS_REMOVE: f->eax = syscall_remove (ARG(1)); break;
//    case SYS_OPEN: f->eax = syscall_open (ARG(1)); break;
//    case SYS_FILESIZE: f->eax = syscall_filesize (ARG(1)); break;
//    case SYS_READ: f->eax = syscall_read (ARG(1), ARG(2), ARG(3)); break;
//    case SYS_WRITE: f->eax = syscall_write (ARG(1), ARG(2), ARG(3)); break;
//    case SYS_SEEK: syscall_seek (ARG(1), ARG(2)); break;
//    case SYS_TELL: f->eax = syscall_tell (ARG(1)); break;
//    case SYS_CLOSE: syscall_close (ARG(1)); break;
    default: ASSERT(false); break;
  } 
}
