#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "devices/disk.h"
#include "threads/synch.h"
#include "userprog/syscall.h"
#include "threads/palloc.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44
#define PT_PER_SECTOR (DISK_SECTOR_SIZE / sizeof (disk_sector_t))

struct inode_child
{
  disk_sector_t pt[PT_PER_SECTOR];
};

/* On-disk inode.
   Must be exactly DISK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    disk_sector_t child;
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
    int type;                           /* 0: file, 1: directory */
    int unused[124];
  };

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, DISK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    disk_sector_t sector;               /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /* Inode content. */
  };

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   disk.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (disk_sector_t sector, off_t length, enum file_status type)
{
  bool isLockAcquired = false;
  if (lock_held_by_current_thread (&file_lock) == false)
  {
    lock_acquire (&file_lock);
    isLockAcquired = true;
  }

  struct inode_disk *disk_inode = NULL;
  disk_sector_t child;
  struct inode_child *ic = palloc_get_page (PAL_ZERO);
  struct inode_child *ic2 = palloc_get_page (PAL_ZERO);
  bool success = true;
  int i, j;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == DISK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);

  if (disk_inode == NULL) success = false;

  if (free_map_allocate (1, &child) == false) return false;
  disk_inode->child = child;
  disk_inode->length = length;
  disk_inode->magic = INODE_MAGIC;
  disk_inode->type = type;
  disk_write (filesys_disk, sector, disk_inode);

  int lv1 = length / PT_PER_SECTOR / DISK_SECTOR_SIZE;
  int lv2 = (length % (PT_PER_SECTOR * DISK_SECTOR_SIZE)) / DISK_SECTOR_SIZE;

  for (i = 0; i < lv1; i++)
  {
    if (free_map_allocate (1, &child) == false) return false;
    ic->pt[i] = child;

    for (j = 0; j < PT_PER_SECTOR; j++)
    {
      if (free_map_allocate (1, &child) == false) return false;
      ic2->pt[j] = child;  
    }

    disk_write (filesys_disk, ic->pt[i], ic2);
  }

  if (free_map_allocate (1, &child) == false) return false;
  ic->pt[lv1] = child;
  
  for (j = 0; j <= lv2; j++)
  {
      if (free_map_allocate (1, &child) == false) return false;
      ic2->pt[j] = child; 
  }
  for (; j < PT_PER_SECTOR; j++) ic2->pt[j] = NULL;

  disk_write (filesys_disk, ic->pt[lv1], ic2);
   
  disk_write (filesys_disk, disk_inode->child, ic);

  palloc_free_page (ic);
  palloc_free_page (ic2);
 
  if (isLockAcquired == true) lock_release (&file_lock);
 
  // palloc = free_map_allocate
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (disk_sector_t sector) 
{
//  printf ("%d opened\n", sector);

  bool isLockAcquired = false;
  if (lock_held_by_current_thread (&file_lock) == false)
  {
    lock_acquire (&file_lock);
    isLockAcquired = true;
  }

  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          if (isLockAcquired == true) lock_release (&file_lock);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
  {
    if (isLockAcquired == true) lock_release (&file_lock);
    return NULL;
  }

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  disk_read (filesys_disk, inode->sector, &inode->data);

  if (isLockAcquired == true) lock_release (&file_lock);

  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  bool isLockAcquired = false;
  if (lock_held_by_current_thread (&file_lock) == false)
  {
    lock_acquire (&file_lock);
    isLockAcquired = true;
  }

  if (inode != NULL)
    inode->open_cnt++;

  if (isLockAcquired == true) lock_release (&file_lock);

  if (inode->removed) return NULL;

  return inode;
}

/* Returns INODE's inode number. */
disk_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

enum file_status
inode_get_type (const struct inode *inode)
{
  return inode->data.type;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  bool isLockAcquired = false;
  if (lock_held_by_current_thread (&file_lock) == false)
  {
    lock_acquire (&file_lock);
    isLockAcquired = true;
  }

  /* Ignore null pointer. */
  if (inode == NULL)
  {
    if (isLockAcquired == true) lock_release (&file_lock);
    return;
  }

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          int length = inode->data.length;
          int lv1 = length / PT_PER_SECTOR / DISK_SECTOR_SIZE;
          int lv2 = (length % (PT_PER_SECTOR * DISK_SECTOR_SIZE)) / DISK_SECTOR_SIZE;
          int i, j;
          struct inode_child *ic = palloc_get_page (PAL_ZERO);
          struct inode_child *ic2 = palloc_get_page (PAL_ZERO);

          disk_read (filesys_disk, inode->data.child, ic);
          free_map_release (inode->data.child, 1);

          for (i = 0; i < lv1; i++)
          {
            disk_read (filesys_disk, ic->pt[i], ic2);
            free_map_release (ic->pt[i], 1);

            for (j = 0; j < PT_PER_SECTOR; j++)
            {
              free_map_release (ic2->pt[j], 1);
            }
          }

          disk_read (filesys_disk, ic->pt[lv1], ic2);
          free_map_release (ic->pt[lv1], 1);

          for (j = 0; j <= lv2; j++)
          {
            free_map_release (ic2->pt[j], 1);
          }

          palloc_free_page (ic);
          palloc_free_page (ic2);
        }
      
      free (inode); 
    }
  
  if (isLockAcquired == true) lock_release (&file_lock);
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  bool isLockAcquired = false;
  if (lock_held_by_current_thread (&file_lock) == false)
  {
    lock_acquire (&file_lock);
    isLockAcquired = true;
  }

  ASSERT (inode != NULL);
  inode->removed = true;

  if (isLockAcquired == true) lock_release (&file_lock);
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
/*   bool isLockAcquired = false;
  if (lock_held_by_current_thread (&file_lock) == false)
  {
    lock_acquire (&file_lock);
    isLockAcquired = true;
  } */

  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;
  struct inode_child *tmp1 = palloc_get_page (PAL_ZERO);
  struct inode_child *tmp2 = palloc_get_page (PAL_ZERO);
  bool zero_sector;

  while (size > 0) 
    {
      if (inode_length (inode) < offset) break;

      int lv1 = offset / PT_PER_SECTOR / DISK_SECTOR_SIZE;
      int lv2 = (offset % (PT_PER_SECTOR * DISK_SECTOR_SIZE)) / DISK_SECTOR_SIZE;
      disk_sector_t sector_idx;

      zero_sector = false;

      disk_read (filesys_disk, inode->data.child, tmp1);
    
      if (tmp1->pt[lv1] == NULL) zero_sector = true;
      else
      {
        disk_read (filesys_disk, tmp1->pt[lv1], tmp2);
        sector_idx = tmp2->pt[lv2];
        if (sector_idx == NULL) zero_sector = true;
      }

      /* Disk sector to read, starting byte offset within sector. */
//      disk_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % DISK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = DISK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (zero_sector == true)
      {
        memset (buffer + bytes_read, 0, chunk_size);
      }

      else if (sector_ofs == 0 && chunk_size == DISK_SECTOR_SIZE) 
      {
        /* Read full sector directly into caller's buffer. */
        disk_read (filesys_disk, sector_idx, buffer + bytes_read); 
      }

      else 
      {
        /* Read sector into bounce buffer, then partially copy
           into caller's buffer. */
        if (bounce == NULL) 
        {
          bounce = malloc (DISK_SECTOR_SIZE);
          if (bounce == NULL)
            break;
        }
        disk_read (filesys_disk, sector_idx, bounce);
        memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
      }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);

  palloc_free_page (tmp1);
  palloc_free_page (tmp2);

//  if (isLockAcquired == true) lock_release (&file_lock);

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
//  printf ("write at %x size%d\n", inode, size);

/*  bool isLockAcquired = false;
  if (lock_held_by_current_thread (&file_lock) == false)
  {
    lock_acquire (&file_lock);
    isLockAcquired = true;
  } */

  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;
  struct inode_child *tmp1 = palloc_get_page(PAL_ZERO);
  struct inode_child *tmp2 = palloc_get_page(PAL_ZERO);
  int i;

  if (inode->deny_write_cnt)
  {
//    if (isLockAcquired == true) lock_release (&file_lock);
    return 0;
  }

  if (inode_length (inode) < offset + size) inode->data.length = offset + size;
  disk_write (filesys_disk, inode->sector, &inode->data);
  
  while (size > 0) 
    {
      int lv1 = offset / PT_PER_SECTOR / DISK_SECTOR_SIZE;
      int lv2 = (offset % (PT_PER_SECTOR * DISK_SECTOR_SIZE)) / DISK_SECTOR_SIZE;

      disk_read (filesys_disk, inode->data.child, tmp1);

      if (tmp1->pt[lv1] == NULL) 
      {
        struct inode_child *ic = palloc_get_page (PAL_ZERO);
        disk_sector_t child;
        if (free_map_allocate (1, &child) == false) return false;

        for (i = 0; i < PT_PER_SECTOR; i++) ic->pt[i] = NULL; 
        disk_write (filesys_disk, child, ic);  // fill with NULL 
  
        tmp1->pt[lv1] = child;
        disk_write (filesys_disk, inode->data.child, tmp1);
      }

      disk_read (filesys_disk, tmp1->pt[lv1], tmp2);

      if (tmp2->pt[lv2] == NULL)
      {
        disk_sector_t child;
        if (free_map_allocate (1, &child) == false) return false;
        tmp2->pt[lv2] = child;
        disk_write (filesys_disk, tmp1->pt[lv1], tmp2);
      }

      disk_sector_t sector_idx = tmp2->pt[lv2];

      /* Sector to write, starting byte offset within sector. */
//      disk_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % DISK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = DISK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == DISK_SECTOR_SIZE) 
        {
          /* Write full sector directly to disk. */
          disk_write (filesys_disk, sector_idx, buffer + bytes_written); 
        }
      else 
        {
          /* We need a bounce buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (DISK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          if (sector_ofs > 0 || chunk_size < sector_left) 
            disk_read (filesys_disk, sector_idx, bounce);
          else
            memset (bounce, 0, DISK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          disk_write (filesys_disk, sector_idx, bounce); 
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);
  palloc_free_page (tmp1);
  palloc_free_page (tmp2);

 // if (isLockAcquired == true) lock_release (&file_lock);

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{ 
  bool isLockAcquired = false;
  if (lock_held_by_current_thread (&file_lock) == false)
  {
    lock_acquire (&file_lock);
    isLockAcquired = true;
  }

  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);

  if (isLockAcquired == true) lock_release (&file_lock);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  bool isLockAcquired = false;
  if (lock_held_by_current_thread (&file_lock) == false)
  {
    lock_acquire (&file_lock);
    isLockAcquired = true;
  }

  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;

  if (isLockAcquired == true) lock_release (&file_lock);
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}
