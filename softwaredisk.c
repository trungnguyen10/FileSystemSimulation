//
// Software disk implementation for LSU 4103 filesystem assignment.
// Written by Golden G. Richard III (@nolaforensix), 10/2017.
//

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include "softwaredisk.h"

#define NUM_BLOCKS 5000
#define BACKING_STORE "sdprivate.sd"

// internals of software disk implementation
typedef struct SoftwareDiskInternals {
  FILE *fp;       
} SoftwareDiskInternals;

//
// GLOBALS
//

static SoftwareDiskInternals sd;

// software disk error code set (set by each software disk function).
SDError sderror;


// initializes the software disk to all zeros, destroying any existing
// data.  Returns 1 on success, otherwise 0. Always sets global 'sderror'.
int init_software_disk() {
  int i;
  char block[SOFTWARE_DISK_BLOCK_SIZE];
  sderror=SD_NONE;
  sd.fp=fopen(BACKING_STORE, "w+");
  if (! sd.fp) {
    sderror=SD_INTERNAL_ERROR;
    return 0;
  }
  
  bzero(block, SOFTWARE_DISK_BLOCK_SIZE);
  for (i=0; i < NUM_BLOCKS; i++) {
    if (fwrite(block, SOFTWARE_DISK_BLOCK_SIZE, 1, sd.fp) != 1) {
      fclose(sd.fp);
      sd.fp=NULL;
      sderror=SD_INTERNAL_ERROR;
      return 0;
    }
  }
  return 1;
}

// returns the size of the SoftwareDisk in multiples of SOFTWARE_DISK_BLOCK_SIZE
unsigned long software_disk_size() {

  return NUM_BLOCKS;
}

// writes a block of data from 'buf' at location 'blocknum'.  Blocks are numbered 
// from 0.  The buffer 'buf' must be of size SOFTWARE_DISK_BLOCK_SIZE.  Returns 1
// on success or 0 on failure.  Always sets global 'sderror'.
int write_sd_block(void *buf, unsigned long blocknum) {

  sderror=SD_NONE;
  if (! sd.fp) {
    sd.fp=fopen(BACKING_STORE, "r+");
    if (! sd.fp) {             
      sderror=SD_INTERNAL_ERROR;
      return 0;
    }
    else {
      fseek(sd.fp, 0L, SEEK_END);
      if (ftell(sd.fp) != NUM_BLOCKS * SOFTWARE_DISK_BLOCK_SIZE) {
	fclose(sd.fp);
	sd.fp=0;
	sderror=SD_NOT_INIT;
	return 0;
      }
    }
  }

  if (blocknum > NUM_BLOCKS-1) {
    sderror=SD_ILLEGAL_BLOCK_NUMBER;
    return 0;
  }

  fseek(sd.fp, blocknum * SOFTWARE_DISK_BLOCK_SIZE, SEEK_SET);
  if (fwrite(buf, SOFTWARE_DISK_BLOCK_SIZE, 1, sd.fp) != 1) {
    sderror=SD_INTERNAL_ERROR;
    return 0;
  }
  fflush(sd.fp);
  return 1;
}

// reads a block of data into 'buf' from location 'blocknum'.  Blocks are numbered 
// from 0.  The buffer 'buf' must be of size SOFTWARE_DISK_BLOCK_SIZE.  Returns 1
// on success or 0 on failure.  Always sets global 'sderror'.
int read_sd_block(void *buf, unsigned long blocknum) {

  sderror=SD_NONE;
  if (! sd.fp) {
    sd.fp=fopen(BACKING_STORE, "r+");
    if (! sd.fp) {             
      sderror=SD_INTERNAL_ERROR;
      return 0;
    }
    else {
      fseek(sd.fp, 0L, SEEK_END);
      if (ftell(sd.fp) != NUM_BLOCKS * SOFTWARE_DISK_BLOCK_SIZE) {
	fclose(sd.fp);
	sd.fp=0;
	sderror=SD_NOT_INIT;
	return 0;
      }
    }
  }

  if (blocknum > NUM_BLOCKS-1) {
    sderror=SD_ILLEGAL_BLOCK_NUMBER;
    return 0;
  }

  fseek(sd.fp, blocknum * SOFTWARE_DISK_BLOCK_SIZE, SEEK_SET);
  if (fread(buf, SOFTWARE_DISK_BLOCK_SIZE, 1, sd.fp) != 1) {
    sderror=SD_INTERNAL_ERROR;
    return 0;
  }
  fflush(sd.fp);
  return 1;
}

// describe current software disk error code by printing a descriptive message to
// standard error.
void sd_print_error(void) {
  switch (sderror) {
  case SD_NONE:
    printf("SD: No error.\n");
    break;
  case SD_NOT_INIT:
    printf("SD: Software disk not initialized.\n");
    break;
  case SD_ILLEGAL_BLOCK_NUMBER:
    printf("SD: Illegal block number.\n");
    break;
  case SD_INTERNAL_ERROR:
    printf("SD: Internal error, software disk unusuable.\n");
    break;
  default:
    printf("SD: Unknown error code %d.\n", sderror);
  }
}

// software disk  error code set (set by each software disk function).
SDError sderror;
