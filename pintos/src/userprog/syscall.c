#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}
int write(int fd, const void* buffer, unsigned size);
static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  int fd;
  void* buffer;
  unsigned size;
  int *sp = f->esp;
  int opt = *sp++;
  switch(opt) {
    case SYS_HALT:
      break;
    case SYS_EXIT:
      thread_current()->exit_status = *sp;
      thread_exit ();
      break;
    case SYS_EXEC:
      break;
    case SYS_WAIT:
      break;
    case SYS_CREATE:
      break;
    case SYS_REMOVE:
      break;
    case SYS_OPEN:
      break;
    case SYS_FILESIZE:
      break;
    case SYS_READ:
      break;
    case SYS_WRITE:
      fd = *((int*)f->esp + 1);
      buffer = (void*) (*((int*)f->esp + 2));
      size = *((unsigned*)f->esp + 3);      
      f->eax = write(fd, buffer, size);
      break;
    case SYS_SEEK:
      break;
    case SYS_TELL:
      break;
    case SYS_CLOSE:
      break;
    
  }
  //thread_exit ();
}
int write(int fd, const void* buffer, unsigned size) 
{
  if(fd == 1)
  {
    putbuf(buffer, size);
    return size;
  }
  return size;
}
