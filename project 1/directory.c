#include "directory.h"
#include "inode.h"
#include "diskimg.h"
#include "file.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

/**
 * Looks up the specified name (name) in the specified directory (dirinumber).  
 * If found, return the directory entry in space addressed by dirEnt.  Returns 0 
 * on success and something negative on failure. 
 */
int directory_findname(struct unixfilesystem *fs, const char *name, int dirinumber, struct direntv6 *dirEnt){
	struct inode i;
  // Error checking 1: Error fetching inode 
	int err = inode_iget(fs, dirinumber, &i);
	if (err < 0) {
    return err;
  }

  // Error checking 2: Not given a diretory
	if (!(i.i_mode & IALLOC) || ((i.i_mode & IFMT) != IFDIR)) {
    printf("Not a directory.\n");
  	return -1;
	}

	int size = inode_getsize(&i);
	int numBlocks  = (size + DISKIMG_SECTOR_SIZE - 1) / DISKIMG_SECTOR_SIZE;
	char buf[DISKIMG_SECTOR_SIZE];
	struct direntv6 *dir = (struct direntv6 *) buf;
	for (int i=0; i<numBlocks; i++) {
  	int bytesLeft = file_getblock(fs, dirinumber, i, dir);
  	int num = bytesLeft/sizeof(struct direntv6);
  	for (int i=0; i<num ; i++) {
    		if (strcmp(name, dir[i].d_name) == 0) {
      		*dirEnt = dir[i];
      		return 0;
    		}
  	}
	}
	return -1;
}
