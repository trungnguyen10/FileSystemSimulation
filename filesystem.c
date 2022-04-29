#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "softwaredisk.h"
#include "filesystem.h"

// number of entries = number of inodes = number of files
#define NUM_FILES 800
#define NUM_BYTES_PER_ADDRESS 4

// DIR SPECS
#define ENTRY_SIZE 64
#define NUM_BYTES_PER_FILENO 3

// INODE SPECS
#define INODE_SIZE 64
#define NUM_DIRECT_BLOCK 12
#define NUM_SINGLE_INDIRECT 1

// DEFINITION OF STRUCTS

// main private file type: you implement this in filesystem.c
struct FileInternals
{
    unsigned long file_no;
    unsigned long cur_pos;
    unsigned long eof_pos; // COULD BE IN INODES
    FileMode mode;
};

typedef struct DirStruct
{
    int start_block;
    int num_entries_per_block;
    int num_blocks_for_Dir;
    int size;
    char entries[NUM_FILES][ENTRY_SIZE];
} DirStruct;

typedef struct InodesStruct
{
    int start_block;
    int num_inodes_per_block;
    int num_blocks_for_inodes;
    int size;
    char list_inodes[NUM_FILES][INODE_SIZE];
} InodesStruct;

typedef struct BitMap
{
    int start_block;
    int max_block;
    int size;
    char map[SOFTWARE_DISK_BLOCK_SIZE];
} BitMap;

// GLOBALS
// intance of directory
// instance of inodes
// instance of bitmap

static InodesStruct inodes;
static DirStruct dir;
static BitMap bitmap;

FSError fserror;

/////////////////////////////// HELPER FUNCTIONS ///////////////////////////////
////////////////////////////////////////////////////////////////////////////////

// SPECS
void print_specs()
{
    printf("dir start_block %d\n", dir.start_block);
    printf("dir num_entries_per_block %d\n", dir.num_entries_per_block);
    printf("dir num_blocks_for_Dir %d\n", dir.num_blocks_for_Dir);
    printf("dir size %d\n", dir.size);
    printf("-----------------------------------------------\n");
    printf("inodes start_block %d\n", inodes.start_block);
    printf("inodes num_inodes_per_block %d\n", inodes.num_inodes_per_block);
    printf("inodes num_blocks_for_inodes %d\n", inodes.num_blocks_for_inodes);
    printf("inodes size %d\n", inodes.size);
    printf("-----------------------------------------------\n");
    printf("bitmap start_block %d\n", bitmap.start_block);
    printf("bitmap max_block %d\n", bitmap.max_block);
    printf("bitmap size %d\n", bitmap.size);
}

////////////// DIR OPERATIONS //////////////

// load the corresponding blocks from disk into the DIR structure
int load_dir_from_disk()
{
    char buf[SOFTWARE_DISK_BLOCK_SIZE];
    int success = 0;
    dir.size = 0;
    for (int z = 0; z < dir.num_blocks_for_Dir; z++)
    {
        // read each block to buf
        success = read_sd_block(buf, dir.start_block + z);
        if (success)
        {
            for (int i = 0; i < dir.num_entries_per_block; i++)
            {
                if (buf[i * ENTRY_SIZE] != '\0')
                    dir.size++;
                for (int j = 0; j < ENTRY_SIZE; j++)
                {
                    dir.entries[z * dir.num_entries_per_block + i][j] = buf[j + i * ENTRY_SIZE];
                }
            }
        }
        else
            break;
    }
    return success;
}

// write an entry at specified index to disk
int write_entry_to_disk(int index)
{
    int success = 0;
    if (index < dir.size && index >= 0)
    {
        char buf[SOFTWARE_DISK_BLOCK_SIZE];
        int target_block_index = (int)(index / dir.num_entries_per_block) + dir.start_block;
        int target_segment_index = index * ENTRY_SIZE - (target_block_index - dir.start_block) * SOFTWARE_DISK_BLOCK_SIZE;

        success = read_sd_block(buf, (unsigned long)target_block_index);
        if (success)
        {
            for (int k = 0; k < ENTRY_SIZE; k++)
            {
                buf[k + target_segment_index] = dir.entries[index][k];
            }
        }
        else
            return success;

        success = write_sd_block(buf, (unsigned long)target_block_index);
    }
    return success;
}

// get entry with the given filename, return the index of entry(or inode), return -1 when not found
int get_entry(char *filename)
{
    if (dir.size == 0)
        return -1;
    else
    {
        const int number = ENTRY_SIZE - NUM_BYTES_PER_FILENO;
        char entry_name[number];
        for (int i = 0; i < dir.size; i++)
        {
            strncpy(entry_name, dir.entries[i], number);
            if (!(strcmp(filename, entry_name)))
            {
                char index[NUM_BYTES_PER_FILENO];
                for (int j = 0; j < NUM_BYTES_PER_FILENO; j++)
                {
                    index[j] = dir.entries[i][ENTRY_SIZE - NUM_BYTES_PER_FILENO + j];
                }
                return atoi(index);
            }
        }
        return -1;
    }
}

// return the number of used entry
int num_used_entries()
{
    return dir.size;
}

// delete an entry at index
int delete_entry(int index)
{
    int success = 0;
    if (index >= 0 && index < dir.size)
    {
        // get pointer to the last entry
        char *last_entry = dir.entries[dir.size - 1];

        // copy last entry filename to the deleted entry filename
        for (int i = 0; i < ENTRY_SIZE - NUM_BYTES_PER_FILENO; i++)
        {
            dir.entries[index][i] = last_entry[i];
        }

        // copy index to file number of the entry
        last_entry = last_entry + (ENTRY_SIZE - NUM_BYTES_PER_FILENO);
        sprintf(last_entry, "%d", index);

        // delete associated inode
        success = delete_inode(index);

        // mark last entry invalid
        last_entry[0] = '\0';

        // update size
        dir.size--;
        return success;
    }
    else
        return success;
}

// add an new entry with given filename
int add_entry(char *filename)
{
    int success = 0;
    if (dir.size < NUM_FILES)
    {
        int index = dir.size;

        // copy filename
        strncpy(dir.entries[index], filename, ENTRY_SIZE - NUM_BYTES_PER_FILENO);

        // copy file number
        char *fileno = dir.entries[index];
        fileno = fileno + (ENTRY_SIZE - NUM_BYTES_PER_FILENO);
        sprintf(fileno, "%d", index);

        // create new inode associated to entry
        success = add_inode();

        // increase size
        dir.size++;
        return success;
    }
    else
        return success;
}

// print entry at index
void print_entry(int index)
{
    for (int i = 0; i < ENTRY_SIZE; i++)
    {
        printf("%c", dir.entries[index][i]);
    }
    printf("\n");
}

// init Dir structure
int init_dir()
{
    dir.start_block = 0;
    dir.num_entries_per_block = SOFTWARE_DISK_BLOCK_SIZE / ENTRY_SIZE; // 8
    dir.num_blocks_for_Dir = NUM_FILES / dir.num_entries_per_block;    // 100
    int success = load_dir_from_disk();
    return success;
}

////////////// INODES OPERATIONS //////////////

// load the corresponding blocks from disk into the inodes structure
int load_inodes_from_disk()
{
    char buf[SOFTWARE_DISK_BLOCK_SIZE];
    int success = 0;
    inodes.size = 0;
    for (int z = 0; z < inodes.num_blocks_for_inodes; z++)
    {
        // read each block to buf
        success = read_sd_block(buf, inodes.start_block + z);
        if (success)
        {
            for (int i = 0; i < inodes.num_inodes_per_block; i++)
            {
                if (buf[i * INODE_SIZE] != '\0')
                    inodes.size++;
                for (int j = 0; j < INODE_SIZE; j++)
                {
                    inodes.list_inodes[z * inodes.num_inodes_per_block + i][j] = buf[j + i * INODE_SIZE];
                }
            }
        }
        else
            break;
    }
    return success;
}

// load the content of the inode at index into buf
int read_inode(char *buf, int index)
{
    if (index < inodes.size && index >= 0)
    {
        for (int k = 0; k < INODE_SIZE; k++)
        {
            buf[k] = inodes.list_inodes[index][k];
        }
        return 1;
    }
    else
        return 0;
}

// return the number of used inodes
int num_used_inodes()
{
    return inodes.size;
}

// write an inode at specified index to disk
int write_inode_to_disk(int index)
{
    int success = 0;
    if (index < inodes.size && index >= 0)
    {
        char buf[SOFTWARE_DISK_BLOCK_SIZE];
        int target_block_index = (int)(index / inodes.num_inodes_per_block) + inodes.start_block;
        int target_segment_index = index * INODE_SIZE - (target_block_index - inodes.start_block) * SOFTWARE_DISK_BLOCK_SIZE;

        success = read_sd_block(buf, (unsigned long)target_block_index);
        if (success)
        {
            for (int k = 0; k < INODE_SIZE; k++)
            {
                buf[k + target_segment_index] = inodes.list_inodes[index][k];
            }
        }
        else
            return success;

        success = write_sd_block(buf, (unsigned long)target_block_index);
    }
    return success;
}

// write data to inode at index
int write_inode(char *data, int index)
{
    if (index < inodes.size && index >= 0)
    {
        for (int i = 0; i < INODE_SIZE; i++)
        {
            inodes.list_inodes[index][i] = data[i];
        }
        return 1;
    }
    else
        return 0;
}

// delete an inode at index
int delete_inode(int index)
{
    if (index < inodes.size && index >= 0)
    { // copy the last inode to the postion of deleted inode, then update size
        char *last_node = inodes.list_inodes[inodes.size - 1];
        for (int i = 0; i < INODE_SIZE; i++)
        {
            inodes.list_inodes[index][i] = last_node[i];
        }
        // mark last_node as invalid
        last_node[0] = '\0';

        // update size
        inodes.size--;
        return 1;
    }
    else
        return 0;
}

// add an empty inode
int add_inode()
{
    if (inodes.size < NUM_FILES)
    {
        int index = inodes.size;
        inodes.size++;
        inodes.list_inodes[index][0] = '0';
        inodes.list_inodes[index][1] = '0';
        return 1;
    }
    else
        return 0;
}

// print inode at index
void print_inode(int index)
{
    for (int i = 0; i < INODE_SIZE; i++)
    {
        printf("%c", inodes.list_inodes[index][i]);
    }
    printf("\n");
}

// init inodes structure
int init_inodes()
{
    inodes.start_block = NUM_FILES * ENTRY_SIZE / SOFTWARE_DISK_BLOCK_SIZE; // 100
    inodes.num_inodes_per_block = SOFTWARE_DISK_BLOCK_SIZE / INODE_SIZE;    // 8
    inodes.num_blocks_for_inodes = NUM_FILES / inodes.num_inodes_per_block; // 100
    int success = load_inodes_from_disk();
    return success;
}

////////////// BITMAP OPERATIONS //////////////

// set bit k_th in bitmap.map
void set_bit(int k)
{
    int i = k % 8;
    int pos = k / 8;
    char flag = 1;
    flag = flag << pos;
    bitmap.map[i] = bitmap.map[i] | flag;
}

// clear bit k_th in bitmap.map
void clear_bit(int k)
{
    int i = k % 8;
    int pos = k / 8;
    char flag = 1;
    flag = flag << pos;
    flag = ~flag;
    bitmap.map[i] = bitmap.map[i] & flag;
}

// free the disk block at index
int free_block(int index)
{
    if (index > bitmap.start_block && index <= bitmap.max_block)
    {
        int k = index - bitmap.start_block - 1;
        set_bit(k);
        return 1;
    }
    return 0;
}

// set the disk block at index
int set_block(int index)
{
    if (index > bitmap.start_block && index <= bitmap.max_block)
    {
        int k = index - bitmap.start_block - 1;
        clear_bit(k);
        return 1;
    }
    return 0;
}

// get free block, return -1 when disk is full
int get_free_block()
{
    const int WORD_SIZE = 8;
    int number_zero_word = 0;
    int non_zero_word = 0;

    // find non_zero_word and count zero_word
    for (int i = 0; i < SOFTWARE_DISK_BLOCK_SIZE; i++)
    {
        if (bitmap.map[i] == 0)
            number_zero_word++;
        else
        {
            non_zero_word = bitmap.map[i];
            break;
        }
    }

    // find the offset to the first bit in non_zero_word
    int offset = 1;
    unsigned char check = 128;
    for (int i = 0; i < WORD_SIZE; i++)
    {
        if (!(non_zero_word & (check >> i)))
            offset++;
        else
            break;
    }

    int block_numbner = (WORD_SIZE * number_zero_word + offset) + bitmap.start_block - 1;
    if (block_numbner > 4999)
        return -1;
    else
        return block_numbner;
}

// write bitmap to disk
int write_bitmap_to_disk()
{
    return write_sd_block(bitmap.map, (unsigned long)bitmap.start_block);
}

// load the corresponding block from disk to the bitmap structure
int load_bitmap_from_disk()
{
    return read_sd_block(bitmap.map, (unsigned long)bitmap.start_block);
}

// init bitmap structure
int init_bitmap()
{
    int success = 0;
    bitmap.start_block = dir.num_blocks_for_Dir + inodes.num_blocks_for_inodes + NUM_FILES;
    bitmap.max_block = 4999;
    int num_blocks = 5000 - bitmap.start_block - 1;
    bitmap.size = (num_blocks % 8) == 0 ? num_blocks / 8 : num_blocks / 8 + 1;
    // NO file exists, every block is available, set all to 1
    if (inodes.size == 0)
    {
        for (int i = 0; i < bitmap.size; i++)
        {
            bitmap.map[i] = -1;
        }
        success = 1;
    }
    else
    {
        success = load_bitmap_from_disk();
    }

    return success;
}

//////////////////////////////// MAIN INTERFACE ////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void init_fs()
{
    int success = 0;
    fserror = FS_NONE;
    // char buf[SOFTWARE_DISK_BLOCK_SIZE];
    success = init_software_disk();
    if (!success)
        printf("Something wrong with software disk!\n");

    // init dir
    success = init_dir();
    if (!success)
        printf("Something wrong with dir init!\n");

    // init inodes
    success = init_inodes();
    if (!success)
        printf("Something wrong with inodes init!\n");

    // init inodes
    success = init_bitmap();
    if (!success)
        printf("Something wrong with bitmap init!\n");
}