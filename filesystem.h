// file type used by user code
typedef struct FileInternals *File;

// access mode for open_file()
typedef enum
{
  READ_ONLY,
  READ_WRITE
} FileMode;

// error codes set in global 'fserror' by filesystem functions
typedef enum
{
  FS_NONE,
  FS_OUT_OF_SPACE,          // the operation caused the software disk to fill up
  FS_FILE_NOT_OPEN,         // attempted read/write/close/etc. on file that isn’t open
  FS_FILE_OPEN,             // file is already open. Concurrent opens are not
                            // supported and neither is deleting a file that is open.
  FS_FILE_NOT_FOUND,        // attempted open or delete of file that doesn’t exist
  FS_FILE_READ_ONLY,        // attempted write to file opened for READ_ONLY
  FS_FILE_ALREADY_EXISTS,   // attempted creation of file with existing name
  FS_EXCEEDS_MAX_FILE_SIZE, // seek or write would exceed max file size
  FS_ILLEGAL_FILENAME,      // filename begins with a null character
  FS_IO_ERROR               // something really bad happened
} FSError;

// function prototypes for filesystem API

// open existing file with pathname 'name' and access mode 'mode'.  Current file
// position is set at byte 0.  Returns NULL on error. Always sets 'fserror' global.
File open_file(char *name, FileMode mode);

// create and open new file with pathname 'name' and (implied) access
// mode READ_WRITE. The current file position is set at byte 0.
// Returns NULL on error. Always sets 'fserror' global.
File create_file(char *name);

// close 'file'.  Always sets 'fserror' global.
void close_file(File file);

// read at most 'numbytes' of data from 'file' into 'buf', starting at the
// current file position.  Returns the number of bytes read. If end of file is reached,
// then a return value less than 'numbytes' signals this condition. Always sets
// 'fserror' global.
unsigned long read_file(File file, void *buf, unsigned long numbytes);

// write 'numbytes' of data from 'buf' into 'file' at the current file position.
// Returns the number of bytes written. On an out of space error, the return value may be
// less than 'numbytes'.  Always sets 'fserror' global.
unsigned long write_file(File file, void *buf, unsigned long numbytes);

// sets current position in file to 'bytepos', always relative to the
// beginning of file.  Seeks past the current end of file should
// extend the file. Returns 1 on success and 0 on failure.  Always
// sets 'fserror' global.
int seek_file(File file, unsigned long bytepos);

// returns the current length of the file in bytes. Always sets 'fserror' global.
unsigned long file_length(File file);

// deletes the file named 'name', if it exists. Returns 1 on success, 0 on failure.
// Always sets 'fserror' global.
int delete_file(char *name);

// determines if a file with 'name' exists and returns 1 if it exists, otherwise 0.
// Always sets 'fserror' global.
int file_exists(char *name);

// describe current filesystem error code by printing a descriptive message to standard
// error.
void fs_print_error(void);

// filesystem error code set (set by each filesystem function)
extern FSError fserror;

// initiate filesystem
void init_fs();

///////////////////////////////////////////////
///////////// HELPER FUNCTIONS ////////////////
///////////////////////////////////////////////

// SPECS
void print_specs();

//////// DIR OPERATIONS ////////////

// load the corresponding blocks from disk into the DIR structure
int load_dir_from_disk();

// write an entry at specified index to disk
int write_entry_to_disk(int index);

// get entry with the given filename, return the index of entry(inode), return -1 when not found
int get_entry(char *filename);

// return the number of used entry
int num_used_entries();

// delete an entry at index
int delete_entry(int index);

// add an new entry with given filename, return the index of new entry, return -1  max file number exceeded or illegal filename
int add_entry(char *filename);

// print entry at index
void print_entry(int index);

//////// INODES OPERATIONS ////////////
// load the corresponding blocks from disk into the inodes structure
int load_inodes_from_disk();

// write an inode at specified index to disk
int write_inode_to_disk(int index);

// load the content of the inode at index into buf
int read_inode(char *buf, int index);

// write data to inode at index
int write_inode(char *data, int index);

// return the number of used inodes
int num_used_inodes();

// delete an inode at index
int delete_inode(int index);

// add an empty inode
int add_inode();

// print inode at index
void print_inode(int index);

// return the size of file with given inode
int get_size_in_inode(char *inode_data);

// set the size of file to given inode
int set_size_in_inode(char *inode_data, int size);

// return the number of blocks with given inode
int get_blocks_in_inode(char *inode_data);

// set the number of blocks to given inode
int set_blocks_in_inode(char *inode_data, int blocks);

//////// BITMAP OPERATIONS ////////////

// write bitmap to disk
int write_bitmap_to_disk();

// load the corresponding block from disk to the bitmap structure
int load_bitmap_from_disk();

// free the disk block at index
int free_block(int index);

// set the disk block at index
int set_block(int index);

// return index of a free block, return -1 when disk is full
int get_free_block();