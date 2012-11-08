#include <bitmap.h>


/* only one swap table neccessary 
 * thus no need for make structure?????????
struct swap {
    struct lock lock;   // for lock related to swap table
    struct bitmap *used_slot;

}
*/

struct bitmap *swap_slot;       // 


void swap_init (void);
void *swap_get_slot (enum palloc_flags);
void *swap_get_multiple (enum palloc_flags, size_t page_cnt);
void swap_free_page (void *);
void swap_free_multiple (void *, size_t page_cnt);


