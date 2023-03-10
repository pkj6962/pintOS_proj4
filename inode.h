#ifndef FILESYS_INODE_H
#define FILESYS_INODE_H

#include <stdbool.h>
#include "filesys/off_t.h"
#include "devices/block.h"

struct bitmap;

void inode_init (void);
//TODO
bool inode_create (block_sector_t, off_t);
//TODO
struct inode *inode_open (block_sector_t);
struct inode *inode_reopen (struct inode *);
block_sector_t inode_get_inumber (const struct inode *);
//TODO
void inode_close (struct inode *);
void inode_remove (struct inode *);
//TODO
off_t inode_read_at (struct inode *, void *, off_t size, off_t offset);
//TODO
off_t inode_write_at (struct inode *, const void *, off_t size, off_t offset);
void inode_deny_write (struct inode *);
void inode_allow_write (struct inode *);
off_t inode_length (const struct inode *);

/* Project4: Implemented Functon */

bool inode_update_file_length (struct inode_disk* inode_disk, off_t start_pos, off_t end_pos);




#endif /* filesys/inode.h */
