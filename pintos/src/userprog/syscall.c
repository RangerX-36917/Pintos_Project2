#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <devices/shutdown.h>
#include <threads/vaddr.h>
#include <filesys/filesys.h>
#include <string.h>
#include <filesys/file.h>
#include <devices/input.h>
#include <threads/palloc.h>
#include <threads/malloc.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "process.h"
#include "pagedir.h"

static void syscall_handler (struct intr_frame *);

static void verify_args (int *, int);
static void verify_address (void *);
struct opened_file * find_file (int);

int read (int, uint8_t* , off_t, int*);
int write (int, uint8_t* , off_t, int);
int open (const char*);
int file_size (int);
void seek (int, off_t);
off_t tell (int);
void close (int);

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/* Find target file according to fd in all opened files of current thread. */
struct opened_file * find_file (int fd)
{
  struct list_elem * elem;
  struct opened_file * file =NULL;

  struct list *files = &thread_current ()->files;
  for (elem = list_begin (files); elem != list_end (files); elem = list_next (elem))
  {
    file = list_entry (elem, struct opened_file, file_elem);
    if (file->fd == fd)
      return file;
  }
  return NULL;
}


static void
syscall_handler (struct intr_frame *f UNUSED)
{
  int * sp = f->esp;
  verify_address ((void *)sp);
  int opt = *sp++;

  switch (opt){
    case SYS_HALT:
      shutdown_power_off ();

    case SYS_EXIT:
      verify_address ((void *)sp);
      thread_current ()->exit_status = *sp;
      thread_exit ();

    case SYS_EXEC:
      verify_address ((void *)sp);
      verify_address ((void*)*sp);
      f->eax = process_execute ((char*)*sp);
      break;

    case SYS_WAIT:
      verify_address ((void *)sp);
      f->eax = process_wait (*sp);
      break;

    case SYS_CREATE:
      verify_args (sp, 2);
      verify_address ((void*)*sp);
      acquire_file_lock();
      f->eax = filesys_create ((const char *)*sp,*(sp+1));
      release_file_lock ();
      break;

    case SYS_REMOVE:
      verify_address ((void *)sp);
      verify_address ((void*)*sp);

      acquire_file_lock ();
      f->eax = filesys_remove ((const char *)*sp);
      release_file_lock ();
      
      break;

    case SYS_OPEN:
      verify_address ((void *)sp);
      verify_address ((void*)*sp);
      f->eax = open ((const char*)*sp);
      break;

    case SYS_FILESIZE:
      verify_address ((void *)sp);
      f->eax = file_size (*sp);
      
      break;

    case SYS_READ:
      verify_args (sp, 3);
      verify_address ((void*)*(sp+1));
      f->eax = read (*sp, (uint8_t*)*(sp+1), *(sp + 2), sp);
      break;

    case SYS_WRITE:
      verify_args (sp, 3);
      verify_address ((void*)*(sp+1));
      f->eax = write (*sp, (const char *)*(sp + 1), *(sp + 2), *sp);
      break;

    case SYS_SEEK:
      verify_args (sp, 2);
      off_t pos = *(sp + 1);
      seek (*sp, pos);
      break;

    case SYS_TELL:
      verify_address ((void *)sp);
      f->eax = tell (*sp);
      break;

    case SYS_CLOSE:
      verify_address ((void *)sp);
      close (*sp);
      break;

    default:
      bad_exit ();
      break;
  }
}

void bad_exit ()
{
  thread_current ()->exit_status = -1;
  thread_exit ();
}

/* Verify multiple arguments. There may be 1-3 arguments. */
void verify_args (int *p, int n)
{
  for (int i = 0; i < n; i++) 
  {
    verify_address ((void *)p++);
  }
}

void verify_address (void *p){
  if  (p == NULL || !is_user_vaddr (p) || !pagedir_get_page (thread_current ()->pagedir,p)) 
    bad_exit ();
}

int read (int fd, uint8_t* buffer, off_t size, int* sp)
{
  off_t read_size;
  if (fd == 0) 
  {
    /* Read from STDIN. */
    for (int i = 0; i < size; i++)
      buffer[i] = input_getc();
    read_size = size;
  }
  else
  {
    /* Not STDIN, we have to find the file throgh its fd. */
    struct opened_file * file_ = find_file(*sp);
    if (file_){
      acquire_file_lock ();
      read_size = file_read (file_->file, buffer, size);
      release_file_lock ();
    } 
    else
    {
      read_size = -1;
    }
  }
  return read_size;
}

int write (int fd, uint8_t* buffer, off_t size, int fd2)
{
  off_t written_size;
  if (fd == 1) 
  {
    /* Write to STDOUT. */
    putbuf (buffer, size);
  }
  else
  {
    /* Write to file. */
    struct opened_file * file_ = find_file (fd2);
    if (file_)
    {
      acquire_file_lock ();
      written_size = file_write (file_->file, buffer, size);
      release_file_lock ();
    } 
    else
    {
      written_size = 0;
    }
  }
  return written_size;
}

int file_size (int fd)
{
  struct opened_file * file_ = find_file (fd);
  off_t size;
  if (file_)
  {
    acquire_file_lock ();
    size = file_length (file_->file);
    release_file_lock ();
  } 
  else
  {
    size = -1;
  }
  return size;
}

int open (const char* file_name)
{
  acquire_file_lock ();
  struct file * file_ =filesys_open (file_name);
  release_file_lock ();

  if (file_)
  {
    struct opened_file *of = malloc (sizeof (struct opened_file));
    of->fd = thread_current ()->fd_next++;
    of->file = file_;
    list_push_back (&thread_current ()->files, &of->file_elem);
    return of->fd;
  }
  else
  {
    return -1;
  }    
}

void seek (int fd, off_t pos)
{
  struct opened_file * file = find_file (fd);
  if (file){
    acquire_file_lock ();
    file_seek (file->file, pos);
    release_file_lock ();
  }
}

off_t tell (int fd)
{
  struct opened_file * file = find_file (fd);
  off_t offset;
  if (file)
  {
    acquire_file_lock ();
    offset = file_tell (file->file);
    release_file_lock ();
  }
  else
  {
    offset = -1;
  }
  return offset;
}

void close (int fd)
{
  struct opened_file * file = find_file (fd);
  if (file)
  {
    acquire_file_lock ();
    file_close (file->file);
    release_file_lock ();
    list_remove (&file->file_elem);
  }
}