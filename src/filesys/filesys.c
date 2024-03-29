#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "devices/disk.h"
#include "filesys/cache.h"
#include "threads/thread.h"

/* The disk that contains the file system. */
struct disk *filesys_disk;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  filesys_disk = disk_get (0, 1);
  if (filesys_disk == NULL)
    PANIC ("hd0:1 (hdb) not present, file system initialization failed");

  inode_init ();
  free_map_init ();

  if (format) 
    do_format ();

  free_map_open ();

  dir_add (dir_open_root (), ".", ROOT_DIR_SECTOR);
  dir_add (dir_open_root (), "..", ROOT_DIR_SECTOR);

  thread_current ()->cwd = dir_open_root ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  free_map_close ();
  cache_destroy ();
}

struct dir*
name_to_dir (const char *name, char *file_name, disk_sector_t *parent)
{
  int i;
  bool isRoot = true;
  for (i = 0; i < strlen (name); i++)
  {
    if (name[i] != '/') 
    {
      isRoot = false;
      break;
    }
  }
  
  if (isRoot == true)
  {
    if (parent != NULL) *parent = ROOT_DIR_SECTOR;
    file_name[0] = '\0';
    return dir_open_root ();
  }

  else if (name[strlen (name)-1] == '/') 
  {
    if (parent != NULL) *parent = ROOT_DIR_SECTOR;
    file_name[0] = 0;
    return NULL;
  }
  
  struct dir *cd;
  
  if (name[0] == '/') cd = dir_open_root ();
  else cd = dir_reopen (thread_current ()->cwd);

  char *tmp = palloc_get_page (0);
  strlcpy (tmp, name, strlen (name) + 1);

  char *save_ptr = NULL;
  char *tok = strtok_r (tmp, "/", &save_ptr);
  if (parent != NULL) *parent = inode_get_inumber( dir_get_inode(cd));

  while (1)
  {
    struct inode *inode = NULL;
    
    if (*save_ptr == NULL) 
    {
      strlcpy (file_name, tok, strlen (tok) + 1); 
      break;
    }
    
    dir_lookup (cd, tok, &inode);
    dir_close (cd);

    if (inode == NULL || inode_get_type (inode) == TYPE_FILE)
    {
      palloc_free_page(tmp);
      return NULL;
    }

   
    cd = dir_open (inode);

    tok = strtok_r (NULL, "/", &save_ptr);
    if (parent != NULL) *parent = inode_get_inumber (inode);
  }

  palloc_free_page (tmp);

  return cd;
}


/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size) 
{
  if (strlen (name) > MAX_CWD_LENGTH) return false;

  disk_sector_t inode_sector = 0;
  char *file_name = (char *)malloc (NAME_MAX+1);
  struct dir *dir = name_to_dir (name, file_name, NULL);

  if (dir == NULL) 
  {
    free (file_name);
    return false;
  }
   
  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size, TYPE_FILE)
                  && dir_add (dir, file_name, inode_sector));
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);

  dir_close (dir);
  free(file_name);

  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
void *
filesys_open (const char *name, bool *is_dir)
{
  if (strcmp (name, "") == 0) return NULL;

  char *file_name = (char *)malloc (NAME_MAX+1);
  struct dir *dir = name_to_dir (name, file_name, NULL);

  //if (dir == NULL || strcmp (file_name, ".") == 0 || strcmp (file_name, "..") == 0) 
  if (dir == NULL ) 
  {
    free (file_name);
    return NULL;
  }

  if (file_name[0] == 0)
  {
    *is_dir = true;
    return dir_open_root ();
  }

  struct inode *inode = NULL;

//  printf ("root:%x,dir:%x,file:%s\n", dir_open_root(), dir, file_name); 

  if (dir != NULL)
    dir_lookup (dir, file_name, &inode);
  dir_close (dir);
  free (file_name);

  if (inode == NULL) return NULL;

  if (inode_get_type (inode) == TYPE_FILE) 
  {
    //printf ("file type\n");
    *is_dir = false;
    return file_open (inode);
  }

  else 
  {
    //printf ("dir type\n");
    *is_dir = true;
    return dir_open (inode);
  }
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  char *file_name = (char *)malloc (NAME_MAX);
  struct dir *dir = name_to_dir (name, file_name, NULL);

  if (dir == NULL) 
  {
    dir_close (dir);
    free (file_name);
    return false;
  }

  bool success = dir != NULL && dir_remove (dir, file_name);
  dir_close (dir); 
  free (file_name);

  return success;
}

bool 
filesys_chdir (const char *name)
{
  char *file_name = (char *)malloc (NAME_MAX);
  struct dir *dir = name_to_dir (name, file_name, NULL);

  if (dir == NULL) 
  {
    dir_close (dir);
    free (file_name);
    return false;
  }

  struct inode *inode = NULL;

  dir_lookup (dir, file_name, &inode);
  dir_close (dir);
  free (file_name);

//  printf ("type:%d\n", inode_get_type (inode));

  if (inode == NULL) 
  {
    
    return false;
  }

  else  if (inode_get_type (inode) == TYPE_DIRECTORY)
  {
    dir_close (thread_current ()->cwd);
    thread_current ()->cwd = dir_open (inode);
    
    return true;
  }

  return false;
}

bool
filesys_mkdir (const char *name)
{
  disk_sector_t inode_sector = 0;
  char *file_name = (char *)malloc (NAME_MAX);
  disk_sector_t parent;  // not NULL
  struct dir *dir = name_to_dir (name, file_name, &parent);

  if (dir == NULL) 
  {
    dir_close (dir);
    free (file_name);
    return false;
  }

  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && dir_create (inode_sector, 0)
                  && dir_add (dir, file_name, inode_sector));

  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);

  else
  {
    struct inode *inode;
    dir_lookup (dir, file_name, &inode);
    if (inode == NULL)
    {
      dir_close(dir);
      free(file_name);
      return false;
    }
    struct dir *new = dir_open (inode);
    
    //printf("mkdir %s\n", name);
    dir_add (new, ".", inode_sector);
    dir_add (new, "..", parent);
    dir_close(new);
  }

  dir_close (dir);
  free(file_name);

  return success;
}

bool
filesys_readdir (struct dir *dir, char *name)
{
  bool success;

  do
  {
    success = false;
    success = dir_readdir (dir, name);
    if (success == false) return success;
  }
  while (strcmp (name, ".") == 0 || strcmp (name, "..") == 0);

  return success;
}


/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}
