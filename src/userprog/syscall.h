#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <stdbool.h>

struct lock file_lock;

void syscall_init (void);

void syscall_halt (void);
void syscall_exit (int);
int syscall_exec (const char *);
int syscall_wait (int);
bool syscall_create (const char *, unsigned int);
bool syscall_remove (const char *);
int syscall_read (int , void *, unsigned);
int syscall_write (int , const void *, unsigned);
int syscall_open (const char *);
int syscall_filesize (int);
void syscall_seek (int , unsigned);
unsigned syscall_tell (int);
void syscall_close (int);
int syscall_mmap (int fd, void *addr);
void syscall_munmap (int);

#endif /* userprog/syscall.h */
