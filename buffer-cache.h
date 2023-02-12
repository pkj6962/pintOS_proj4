#ifndef BUFFER_CACHE_H

#include "threads/synch.h"
#include "devices/block.h"
#include "filesys/off_t.h"

#define BUFFER_ENTRY_NB 64

struct _bc_entry {

    bool present;
    bool dirty;
    bool accessed;

    void * bc_entry_ptr;      /* 디스크 데이터가 캐시되는 실제 메모리 공간 주소*/
    block_sector_t sector;  

    struct lock bc_lock;  
}_bc_entry;

struct _bc_entry buffer_cache[BUFFER_ENTRY_NB];


void buffer_cache_init (void);
void buffer_cache_read (block_sector_t, void*);
void buffer_cache_write (block_sector_t, void *);
struct _bc_entry* buffer_cache_lookup (block_sector_t, int*);
struct _bc_entry* bc_select_victim (int * victim_idx);
void bc_flush_entry (struct _bc_entry *);
void bc_flush_all_entries (void);
void buffer_cache_term (void);

void init_buffer_cache (struct _bc_entry*, bool present, bool dirty, bool accessed, block_sector_t sector);

int bc_find_free_entry_idx (struct bitmap *);
struct _bc_entry* bc_find_free_entry ();




#endif 