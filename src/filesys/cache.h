#include <hash.h>
#include <stdbool.h>
#include <debug.h>
#include "devices/disk.h"
#include "lib/kernel/bitmap.h"

#define CACHE_SIZE 64

struct cache_entry
{
    struct disk *disk;
    disk_sector_t disk_no;
    void *addr;

    bool dirty;
    bool access;

    struct hash_elem hash_elem;
    struct list_elem list_elem;
};

struct cache
{
    void *start_addr;
    struct bitmap *bitmap; 
    struct hash hash;
    struct list list;
}; 

struct cache *cache;

unsigned cache_hash (const struct hash_elem *p_, void *aux UNUSED);
bool cache_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED);

void cache_init ();
void cache_destory ();
struct cache_entry *cache_create (struct disk *disk, disk_sector_t disk_no); 
struct cache_entry *cache_lookup (struct disk* disk, disk_sector_t disk_no);
void cache_delete (struct disk *disk, disk_sector_t disk_no);

