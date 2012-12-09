#include "filesys/cache.h"
#include "vm/swap.h"

unsigned
cache_hash (const struct hash_elem *c_, void *aux UNUSED)
{
  const struct cache_entry *c = hash_entry (c_, struct cache_entry, hash_elem);
  return hash_bytes (&c->disk, (sizeof c->disk_no) + (sizeof c->disk));
}

bool
cache_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED)
{
  const struct cache_entry *a = hash_entry (a_, struct cache_entry, hash_elem);
  const struct cache_entry *b = hash_entry (b_, struct cache_entry, hash_elem);
  
  if (a->disk == b->disk) return a->disk_no < b->disk_no;
  else return a->disk < b->disk;
}

void 
cache_init ()
{
  cache = palloc_get_page (0);  // kernel pool

  //cache->start_addr = malloc ((size_t) CACHE_SIZE * DISK_SECTOR_SIZE);
  cache->start_addr = palloc_get_multiple(0, CACHE_SIZE/SEC_PER_PG);
  cache->bitmap = bitmap_create ((size_t) CACHE_SIZE);
  hash_init (&cache->hash, cache_hash, cache_less, NULL);
  list_init (&cache->list); 
}

void cache_destroy ()
{
  struct list_elem *e;
  struct cache_entry *ce;

  while (list_empty (&cache->list) == false)
  {
    e = list_front (&cache->list);
    ce = list_entry (e, struct cache_entry, list_elem);

    cache_delete (ce->disk, ce->disk_no);
  }

  palloc_free_page (cache);
}

struct cache_entry *cache_create (struct disk *disk, disk_sector_t disk_no)
{
  int slot = bitmap_scan_and_flip (cache->bitmap, 0, 1, false);
  
//  printf ("create %x, %d\n", disk, disk_no);

  if (slot == BITMAP_ERROR) 
  {
    while (1)
    {
      struct list_elem *e = list_pop_front (&cache->list);
      struct cache_entry *ce = list_entry (e, struct cache_entry, list_elem);

      if (ce->access == true)
      {
        ce->access = false;
        list_push_back (&cache->list, &ce->list_elem);
      }

      else
      {
        slot = (ce->addr - cache->start_addr) / DISK_SECTOR_SIZE; // addr to slot_no
        list_push_back (&cache->list, &ce->list_elem); // restore to use cache_delete
        cache_delete (ce->disk, ce->disk_no);
        break;
      }
    }
  }

//  printf ("slot %d\n", slot);

  struct cache_entry *ce = malloc (sizeof (struct cache_entry));

  ce->disk = disk;
  ce->disk_no = disk_no;
  ce->addr = cache->start_addr + slot * DISK_SECTOR_SIZE;
  //printf("CREATE : slot %d, disk %x, sec %d, addr %x\n", slot, ce->disk, ce->disk_no, ce->addr);

  ce->dirty = false;
  ce->access = true;

  hash_insert (&cache->hash, &ce->hash_elem);
  list_push_back (&cache->list, &ce->list_elem);
  bitmap_set (cache->bitmap, slot, true);

  return ce;
}

struct cache_entry *cache_lookup (struct disk *disk, disk_sector_t disk_no)
{
  struct cache_entry ce;
  struct hash_elem *e;

  ce.disk = disk;
  ce.disk_no = disk_no;

  e = hash_find (&cache->hash, &ce.hash_elem);

  return (e == NULL)? NULL: hash_entry (e, struct cache_entry, hash_elem);
}

void cache_delete (struct disk *disk, disk_sector_t disk_no)
{
//  printf ("start delete %x, %d\n", disk, disk_no);
  struct cache_entry ce;
  struct hash_elem *e;
  struct cache_entry *del;

  ce.disk = disk;
  ce.disk_no = disk_no;
  e = hash_find (&cache->hash, &ce.hash_elem);

  ASSERT (e != NULL);

  del = hash_entry (e, struct cache_entry, hash_elem);

  if (del->dirty == true)
  {
    disk_force_write (del->disk, del->disk_no, del->addr);
  }

  hash_delete (&cache->hash, &del->hash_elem);
  list_remove (&del->list_elem);
  bitmap_flip (cache->bitmap, (del->addr - cache->start_addr) / DISK_SECTOR_SIZE);

  free (del);
}  
