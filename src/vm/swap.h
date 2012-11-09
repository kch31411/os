#include <bitmap.h>
#include <stdbool.h>
#include <stddef.h>
#include <inttypes.h>
#include "devices/disk.h"
#include "threads/vaddr.h"
#include "threads/synch.h"

#define SEC_PER_PG (PGSIZE / DISK_SECTOR_SIZE)

struct bitmap *swap_slot;
struct lock swap_lock;
struct disk *swap_disk;

void swap_init (void);
disk_sector_t swap_get_slot (void);
void swap_free_slot (disk_sector_t);
disk_sector_t swap_out (void* phy_addr);
void swap_in (disk_sector_t disk_no, void *phy_addr);

