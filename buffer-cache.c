#include <stdio.h>
#include <stdlib.h>
#include <bitmap.h>

#include "filesys/buffer-cache.h"
#include "devices/block.h"
#include "threads/synch.h"

struct block *fs_device;

/*

Constant Data
#define BUFFER_ENTRY_NB 64

static global data


struct buffer_entry * bc_victim;

*/




// TODO bc_entry의 present 바이트가 false에서 true로 바뀔 때는 bc_entry의 모든 attrbt가 다 바뀌어야 한다.



/* 디스크에서 로드되는 데이터가 캐시되는 실제 메모리 공간 */
struct p_buffer_cache_entry 
{
    char unused[BLOCK_SECTOR_SIZE];
}; 

//TODO 실제 이 구조체가 할당되는 메모리 사이즈는 512 바이트가 아닐 수 있다. 이 때문에 문제가 발생하면 malloc으로 32KB를 할당하는 방식으로 바꿔보자.
struct p_buffer_cache_entry p_buffer_cache[BUFFER_ENTRY_NB];

/*
버퍼 캐시 엔트리들의 할당여부를 관리하는 비트맵
*/
struct bitmap * bc_free_map;
struct lock bc_free_map_lock;


void buffer_cache_init (void)
{
    bc_free_map = bitmap_create (BUFFER_ENTRY_NB);
    if (bc_free_map == NULL)
        PANIC ("Buffer Cache free map allocation is failed.\n");
    
    lock_init(&bc_free_map_lock);

    /* Initialize each buffer cache entry.*/
    for (int i = 0; i < BUFFER_ENTRY_NB; i++)
    {
        buffer_cache[i].present = false; 
        buffer_cache[i].bc_entry_ptr = &p_buffer_cache[i];
        lock_init (&buffer_cache[i].bc_lock);
    }   
}

/* 
Find block sector SECTOR data in buffer cache and copy it to the BUFFER. 
if SECTOR data doesn't exist in buffer cache, then load it from the disk and copy to the buffer.
*/
void 
buffer_cache_read (block_sector_t sector, void * buffer)
{
    int idx,free_idx; 
    struct _bc_entry* bc_entry = buffer_cache_lookup (sector, &idx);
 
    /* There is no target data in buffer cache, so find the empty slot in buffer cache and load data from the disk*/
    if (bc_entry == NULL)
    {
        struct _bc_entry* free_entry = bc_find_free_entry (&free_idx);
        

        lock_acquire (&free_entry->bc_lock);
        
        block_read (fs_device, sector, free_entry->bc_entry_ptr);
        init_buffer_cache (free_entry, true, false, true, sector);
        
        //TODO 데드락 조심해서 비트맵 락 걸기
        ASSERT (bitmap_test(bc_free_map, free_idx) == false); 
        bitmap_flip (bc_free_map, free_idx); 

        printf("What was written: %s\n", (char*)free_entry->bc_entry_ptr);
        memcpy (buffer, free_entry->bc_entry_ptr, BLOCK_SECTOR_SIZE); 

        lock_release (&free_entry->bc_lock);

        
    } 
    else
    {
        lock_acquire (&bc_entry->bc_lock);

        memcpy (buffer, bc_entry->bc_entry_ptr, BLOCK_SECTOR_SIZE); 
        bc_entry->accessed = true; 

        lock_release (&bc_entry->bc_lock);
        
    }

}


/* 
Find SECTOR from buffer cache and write data from BUFFER on it. 
if no entries match SECTOR, then block-read the SECTOR and write on it
(if no free space is available, then victim-out one entry)
*/
void 
buffer_cache_write (block_sector_t sector, void * buffer)
{

    int idx;
    struct _bc_entry * free_entry, * key_entry;
    
    // 1. Find SECTOR from buffer cache.
    key_entry = buffer_cache_lookup (sector, &idx); 

    // 2. if SECTOR is not found in cache, then find the empty slot 
    if (key_entry == NULL)
    {
        key_entry = bc_find_free_entry (&idx);
        ASSERT (bitmap_test (bc_free_map, idx) == false); 
        bitmap_flip (bc_free_map, idx); 
    }

    //3. Load SECTOR data from the disk to the Victim Entry.
    //block_read (fs_device, sector, key_entry->bc_entry_ptr); 
        
    lock_acquire (&key_entry->bc_lock); 

    //4. Finally Write BUFFER data onto the key entry.     
    memcpy (key_entry->bc_entry_ptr, buffer, BLOCK_SECTOR_SIZE); 

    // 5. initialize buffer_cache_entry data
    init_buffer_cache (key_entry, true, true, true, sector); 
    printf("free_idx : %d\n", idx); 


    lock_release (&key_entry->bc_lock); 
    

}


struct _bc_entry* 
buffer_cache_lookup (block_sector_t sector, int *idx)
{
    for (int i = 0; i < BUFFER_ENTRY_NB; i++)
    {
        //TODO Is Synchronization needed?
        if (buffer_cache[i].present == true && buffer_cache[i].sector == sector)
        {
            *idx = i;
            return &buffer_cache[i];
        }
    }
    return NULL; 
}


void init_buffer_cache (struct _bc_entry * bc_entry, bool present, bool dirty, bool accessed, block_sector_t sector)
{
    bc_entry->present = present;
    bc_entry->dirty = dirty;
    bc_entry->accessed = accessed;
    bc_entry->sector = sector;
}


/* Find the free buffer cache entry. If buffer cache is full, 
then find the victim entry by clock algorithm, flush it and return it.*/
struct _bc_entry* 
bc_find_free_entry (int * free_idx)
{

    int idx, victim_idx;

    idx = bc_find_free_entry_idx (bc_free_map);

    printf("idx: %d", idx); 
    if (idx == BITMAP_ERROR) /* no free entry */ 
    {
        struct _bc_entry * victim_entry = bc_select_victim(&victim_idx);
        
        if (victim_entry->dirty == true)
        {
            bc_flush_entry (victim_entry);
        }


        ASSERT ( bitmap_test (bc_free_map, victim_idx) == true);
        bitmap_flip (bc_free_map, victim_idx); // true to false 


        victim_entry->present = false;
        *free_idx = victim_idx; 

        return victim_entry; 
    }   
    else // free buffer cache entry is found. 
    {
        ASSERT ( bitmap_test (bc_free_map, idx) == false);
        *free_idx = idx; 

        return &buffer_cache[idx];
    }
}

int
bc_find_free_entry_idx (struct bitmap* bc_free_map)
{
    
    lock_acquire (&bc_free_map_lock);    
    int idx = bitmap_scan (bc_free_map, 0, 1, false);
    lock_release (&bc_free_map_lock);

    return idx;
}


/* Allocation state change is held in caller function. */
struct _bc_entry*
bc_select_victim (int* victim_idx)
{
    /*
    일단 다른 함수 완성하기 전까지 랜덤(0~63) index bc_entry를 반환하도록 구현. 
    */


   /* Address of VICTIM_IDX is used as random number (pintos stdlib doesn't supply rand() func.) */
    int random_idx = (int)&victim_idx % BUFFER_ENTRY_NB;

    return &buffer_cache[random_idx]; 

}


void 
bc_flush_entry (struct _bc_entry * bc_entry)
{
    /*
    bc_entry.present = false;
    bc_entry.dirty = false;
    */

}




/* When a device is booted-off, flush all the dirty entries and free the related in-memory data.*/
void buffer_cache_term (void)
{
    

    /* free the in-memory data */
    // Is it actually necessary when the hardware is turned off?
    free (bc_free_map);    
}