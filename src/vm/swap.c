#include <debug.h>
#include "vm/swap.h"

void 
swap_init (void)
{
  swap_disk = disk_get (1, 1);
  swap_slot = bitmap_create ((size_t)disk_size (swap_disk));
  lock_init (&swap_lock);
  lock_init (&swap_bitmap_lock);
}

disk_sector_t
swap_get_slot (void)
{
  lock_acquire (&swap_bitmap_lock);
  disk_sector_t ret = bitmap_scan_and_flip (swap_slot, 0, SEC_PER_PG, false);
  lock_release (&swap_bitmap_lock);

  return ret;
}

void
swap_free_slot (disk_sector_t d)
{
  lock_acquire (&swap_bitmap_lock);
  bitmap_set_multiple (swap_slot, (size_t)d, SEC_PER_PG, false);
  lock_release (&swap_bitmap_lock);
}

disk_sector_t
swap_out (void* phy_addr)
{
 // printf("IN swap : %x\n", phy_addr);
  ASSERT (pg_ofs (phy_addr) == 0);
  lock_acquire (&swap_lock);

  int i;
  disk_sector_t ret = swap_get_slot ();

  if (ret == BITMAP_ERROR)
  {
    PANIC ("swap_out: swap disk is full");
  }

  for (i = 0; i < SEC_PER_PG; i++)
  {
    disk_write (swap_disk, ret+i, (phy_addr + DISK_SECTOR_SIZE * i));
  }

  lock_release (&swap_lock);

  return ret;
}

void
swap_in (disk_sector_t disk_no, void *phy_addr)
{
  ASSERT (pg_ofs (phy_addr) == 0);
  lock_acquire (&swap_lock);

  int i;

  for (i = 0; i < SEC_PER_PG; i++)
  {
    disk_read (swap_disk, disk_no+i, (phy_addr + DISK_SECTOR_SIZE * i));
  }
  swap_free_slot (disk_no);

  lock_release (&swap_lock);
}
