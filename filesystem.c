#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "softwaredisk.h"
#include "filesystem.h"

// number of entries = number of inodes = number of files
#define NUM_FILES 800
#define NUM_BYTES_PER_ADDRESS 4
#define MAX_FILE_SIZE 71680
#define MAX_BLOCKS 140

// DIR SPECS
#define ENTRY_SIZE 64
#define NUM_BYTES_PER_FILENO 3

// INODE SPECS
// structure of 1 inode
// |--size(7byte)--|--blocks(5bytes)--|--12_direct_blocks(12*4)--|--1_single_indirect(1*4)--|
// TOTAL 64 bytes
#define INODE_SIZE 64
#define NUM_BYTES_FOR_SIZE 7
#define NUM_BYTES_FOR_BLOCKS 5
#define NUM_DIRECT_BLOCK 12
#define NUM_SINGLE_INDIRECT 1

// DEFINITION OF STRUCTS

// main private file type: you implement this in filesystem.c
typedef struct FileInternals
{
    unsigned long file_no;
    unsigned long cur_pos;
    FileMode mode;
} FileInternals;

typedef struct DirStruct
{
    int start_block;
    int num_entries_per_block;
    int num_blocks_for_Dir;
    int size;
    char entries[NUM_FILES][ENTRY_SIZE];
    int opened_files[NUM_FILES];
} DirStruct;

//////// DIR OPERATIONS ////////////

// load the corresponding blocks from disk into the DIR structure
int load_dir_from_disk();

// write an entry at specified index to disk
int write_entry_to_disk(int index);

// get entry with the given filename, return the index of entry(inode), return -1 when not found
int get_entry(char *filename);

// add file_no to list of opened files, return 1 on success, 0 on error
int add_to_opened_files(int file_no);

// delete file_no from the list of opened files, return 1 on success, 0 on error
int delete_from_opened_files(int file_no);

// check a file with given file_no is opened, return 1 on true, 0 on false
int is_opened(int file_no);

// return the number of used entry
int num_used_entries();

// delete an entry at index, remove file from openned file list, also delete corresponding inode, then write changes to disk
int delete_entry(int index);

// add an new entry with given filename, return the index of new entry, return -1  max file number exceeded or illegal filename, then write changes to disk
int add_entry(char *filename);

// print entry at index
void print_entry(int index);

typedef struct InodesStruct
{
    int start_block;
    int num_inodes_per_block;
    int num_blocks_for_inodes;
    int size;
    char list_inodes[NUM_FILES][INODE_SIZE];
} InodesStruct;

//////// INODES OPERATIONS ////////////

// load the corresponding blocks from disk into the inodes structure
int load_inodes_from_disk();

// write an inode at specified index to disk
int write_inode_to_disk(int index);

// load the content of the inode at index into buf
int read_inode(char *buf, int index);

// write data to inode at index, return 1 for success, 0 for error
int write_inode(char *data, int index);

// return the number of used inodes
int num_used_inodes();

// delete an inode at index, return 1 for success, 0 for error, then write changes to disk
int delete_inode(int index);

// add an empty inode at index, return 1 for success, 0 for error, then write changes to disk
int add_inode(int index);

// print inode at index
void print_inode(int index);

// return the size of file with given inode
int get_size_in_inode(char *inode_data);

// set the size of file to given inode, return 1 for success, 0 for error
int set_size_in_inode(char *inode_data, int size);

// return the number of blocks with given inode
int get_blocks_in_inode(char *inode_data);

// set the number of blocks to given inode, return 1 for success, 0 for error
int set_blocks_in_inode(char *inode_data, int blocks);

// return the block number with given inode and direct block(0-11), return -1 for error. Can be used to get the block num for single indirect(index = 12)
int get_direct_block_num(char *inode_data, int index);

// set the block number at specified direct block(0-11) with given inode, return 1 for success, 0 for error
// can be used to set block number for single indirect(index = 12)
int set_direct_block_num(char *inode_data, int direct_block, int num);

// return the block number with given block and index(0-139), return -1 for error
int get_block_num(char *inode_data, int index);

// set the block number at specified index(0-139) with given num, return 1 for success, 0 for error
int set_block_num(char *inodes_data, int index, int num);

typedef struct BitMap
{
    int start_block;
    int max_block;
    int size;
    int blocks_for_map;
    char map[600];
} BitMap;

//////// BITMAP OPERATIONS ////////////

// init bitmap
int init_bitmap();

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

// GLOBALS
// intance of directory
// instance of inodes
// instance of bitmap

static InodesStruct inodes;
static DirStruct dir;
static BitMap bitmap;
int is_init = 0;

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
    printf("Number of blocks for bitmap %d\n", bitmap.blocks_for_map);
}

////////////// DIR OPERATIONS DEFINITION //////////////
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
                char index[NUM_BYTES_PER_FILENO + 1];
                index[NUM_BYTES_PER_FILENO] = '\0';
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

void print_opened_files()
{
    for (int i = 0; i < NUM_FILES; i++)
    {
        printf("%d ", dir.opened_files[i]);
    }
    printf("\n");
}

int add_to_opened_files(int file_no)
{
    int success = 0;
    if (file_no >= 0 && file_no < NUM_FILES)
    {
        for (int i = 0; i < NUM_FILES; i++)
        {
            if (dir.opened_files[i] == -1)
            {
                dir.opened_files[i] = file_no;
                success = 1;
                break;
            }
        }
    }
    return success;
}

int delete_from_opened_files(int file_no)
{
    int success = 0;
    if (file_no >= 0 && file_no < NUM_FILES)
    {
        for (int i = 0; i < NUM_FILES; i++)
        {
            if (dir.opened_files[i] == file_no)
            {
                dir.opened_files[i] = -1;
                success = 1;
                break;
            }
        }
    }
    return success;
}

int is_opened(int file_no)
{
    int flag = 0;
    if (file_no >= 0 && file_no < NUM_FILES)
    {
        for (int i = 0; i < NUM_FILES; i++)
        {
            if (dir.opened_files[i] == file_no)
            {
                flag = 1;
                break;
            }
        }
    }
    return flag;
}

int num_used_entries()
{
    return dir.size;
}

int delete_entry(int index)
{
    int success = 0;
    if (index >= 0 && index < NUM_FILES)
    {
        for (int i = 0; i < ENTRY_SIZE; i++)
        {
            dir.entries[index][i] = '\0';
        }

        if (is_opened(index))
            delete_from_opened_files(index);

        // update size
        dir.size--;

        success = delete_inode(index);
        if (success)
            success = write_entry_to_disk(index);
    }
    return success;
}

int add_entry(char *filename)
{
    int success = -1;
    if (dir.size < NUM_FILES)
    {
        // check for illegal file name
        if (filename[0] != '\0')
        {
            int char_count = 0;
            while (filename[char_count] != '\0')
            {
                char_count++;
            }
            if (char_count >= 61)
            {
                fserror = FS_ILLEGAL_FILENAME;
                return -1;
            }
            int index = 0;

            for (; index < NUM_FILES; index++)
            {
                if (dir.entries[index][0] == '\0')
                    break;
            }

            // copy filename
            strncpy(dir.entries[index], filename, ENTRY_SIZE - NUM_BYTES_PER_FILENO);

            // copy file number
            char *fileno = dir.entries[index];
            fileno = fileno + (ENTRY_SIZE - NUM_BYTES_PER_FILENO);
            sprintf(fileno, "%d", index);

            // create new inode associated to entry
            add_inode(index);

            // increase size
            dir.size++;

            // write entry to disk
            write_entry_to_disk(index);

            return index;
        }
        else
            fserror = FS_ILLEGAL_FILENAME;
    }
    else
        fserror = FS_OUT_OF_SPACE;

    return success;
}

void print_entry(int index)
{
    for (int i = 0; i < ENTRY_SIZE; i++)
    {
        printf("%c", dir.entries[index][i]);
    }
    printf("\n");
}

// init dir structure
int init_dir()
{
    dir.start_block = 0;
    dir.num_entries_per_block = SOFTWARE_DISK_BLOCK_SIZE / ENTRY_SIZE; // 8
    dir.num_blocks_for_Dir = NUM_FILES / dir.num_entries_per_block;    // 100

    for (int i = 0; i < NUM_FILES; i++)
    {
        dir.opened_files[i] = -1;
    }

    int success = load_dir_from_disk();
    return success;
}

////////////// INODES OPERATIONS DEFINITION //////////////

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

int read_inode(char *buf, int index)
{
    if (index < NUM_FILES && index >= 0)
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

int num_used_inodes()
{
    return inodes.size;
}

int write_inode_to_disk(int index)
{
    int success = 0;
    if (index < NUM_FILES && index >= 0)
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

int write_inode(char *data, int index)
{
    if (index < NUM_FILES && index >= 0)
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

int delete_inode(int index)
{
    int success = 0;
    if (index < NUM_FILES && index >= 0)
    {
        for (int i = 0; i < INODE_SIZE; i++)
        {
            inodes.list_inodes[index][i] = '\0';
        }

        // update size
        inodes.size--;

        success = write_inode_to_disk(index);
    }
    return success;
}

int add_inode(int index)
{
    int success = 0;
    if (index >= 0 && index < NUM_FILES)
    {
        if (inodes.size < NUM_FILES)
        {
            // check if at index is active inode or not
            if (inodes.list_inodes[index][0] == '\0')
            {
                set_size_in_inode(inodes.list_inodes[index], 0);
                set_blocks_in_inode(inodes.list_inodes[index], 0);
                inodes.size++;
                write_inode_to_disk(index);
                success = 1;
            }
        }
    }
    return success;
}

void print_inode(int index)
{
    for (int i = 0; i < INODE_SIZE; i++)
    {
        if (i == 6 || i == 10)
            printf("%c |", inodes.list_inodes[index][i]);
        else
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

int get_size_in_inode(char *inode_data)
{
    char size[NUM_BYTES_FOR_SIZE + 1];
    size[NUM_BYTES_FOR_SIZE] = '\0';
    for (int i = 0; i < NUM_BYTES_FOR_SIZE; i++)
    {
        size[i] = inode_data[i];
    }
    return atoi(size);
}

int set_size_in_inode(char *inode_data, int size)
{
    int success = 0;
    if (size <= MAX_FILE_SIZE)
    {
        char filesize[NUM_BYTES_FOR_SIZE];
        for (int i = 0; i < NUM_BYTES_FOR_SIZE; i++)
        {
            filesize[i] = '\0';
        }
        sprintf(filesize, "%d", size);
        for (int i = 0; i < NUM_BYTES_FOR_SIZE; i++)
        {
            inode_data[i] = filesize[i];
        }
        success = 1;
    }
    return success;
}

int get_blocks_in_inode(char *inode_data)
{
    char blockno[NUM_BYTES_FOR_BLOCKS + 1];
    blockno[NUM_BYTES_FOR_BLOCKS] = '\0';
    for (int i = 0; i < NUM_BYTES_FOR_BLOCKS; i++)
    {
        blockno[i] = inode_data[i + NUM_BYTES_FOR_SIZE];
    }
    return atoi(blockno);
}

int set_blocks_in_inode(char *inode_data, int blocks)
{
    int success = 0;
    if (blocks <= MAX_BLOCKS)
    {
        char blockno[NUM_BYTES_FOR_BLOCKS];
        for (int i = 0; i < NUM_BYTES_FOR_BLOCKS; i++)
        {
            blockno[i] = '\0';
        }
        sprintf(blockno, "%d", blocks);
        for (int i = 0; i < NUM_BYTES_FOR_BLOCKS; i++)
        {
            inode_data[i + NUM_BYTES_FOR_SIZE] = blockno[i];
        }
        success = 1;
    }
    return success;
}

int get_direct_block_num(char *inode_data, int index)
{
    int blocknum = -1;
    if (index >= 0 && index <= NUM_DIRECT_BLOCK)
    {
        // extra space for '\0'
        char num[NUM_BYTES_PER_ADDRESS + 1];
        num[NUM_BYTES_PER_ADDRESS] = '\0';
        for (int i = 0; i < NUM_BYTES_PER_ADDRESS; i++)
        {
            num[i] = inode_data[i + NUM_BYTES_FOR_SIZE + NUM_BYTES_FOR_BLOCKS + index * NUM_BYTES_PER_ADDRESS];
        }
        blocknum = atoi(num);
    }
    return blocknum;
}

int set_direct_block_num(char *inode_data, int direct_block, int num)
{
    int success = 0;
    if (direct_block >= 0 && direct_block <= NUM_DIRECT_BLOCK)
    {
        if (num > bitmap.start_block && num <= bitmap.max_block)
        {
            // extra space for '\0'
            char blocknum[NUM_BYTES_PER_ADDRESS + 1];
            for (int i = 0; i <= NUM_BYTES_PER_ADDRESS; i++)
            {
                blocknum[i] = '\0';
            }
            sprintf(blocknum, "%d", num);
            for (int i = 0; i < NUM_BYTES_PER_ADDRESS; i++)
            {
                inode_data[NUM_BYTES_FOR_SIZE + NUM_BYTES_FOR_BLOCKS + direct_block * NUM_BYTES_PER_ADDRESS + i] = blocknum[i];
            }
            success = 1;
        }
    }
    return success;
}

int get_block_num(char *inode_data, int index)
{
    int blocknum = -1;

    if (index < NUM_DIRECT_BLOCK)
        blocknum = get_direct_block_num(inode_data, index);
    else
    {
        int indirect_block_num = get_direct_block_num(inode_data, 12);
        char block_data[SOFTWARE_DISK_BLOCK_SIZE];
        read_sd_block(block_data, indirect_block_num);
        const int NUM_ADDRESS_PER_BLOCK = SOFTWARE_DISK_BLOCK_SIZE / NUM_BYTES_PER_ADDRESS;
        index = index - NUM_DIRECT_BLOCK;
        if (index < NUM_ADDRESS_PER_BLOCK)
        {
            // extra space for '\0'
            char num[NUM_BYTES_PER_ADDRESS + 1];
            num[NUM_BYTES_PER_ADDRESS] = '\0';
            for (int i = 0; i < NUM_BYTES_PER_ADDRESS; i++)
            {
                num[i] = block_data[index * NUM_BYTES_PER_ADDRESS + i];
            }
            blocknum = atoi(num);
        }
    }
    return blocknum;
}

int set_block_num(char *inode_data, int index, int num)
{
    int success = 0;
    if (index < NUM_DIRECT_BLOCK)
    {
        success = set_direct_block_num(inode_data, index, num);
    }
    else
    {
        const int NUM_ADDRESS_PER_BLOCK = SOFTWARE_DISK_BLOCK_SIZE / NUM_BYTES_PER_ADDRESS;
        index = index - NUM_DIRECT_BLOCK;
        if (index < NUM_ADDRESS_PER_BLOCK)
        {
            if (num > bitmap.start_block && num <= bitmap.max_block)
            {
                int indirect_block_num = get_direct_block_num(inode_data, 12);
                char block_data[SOFTWARE_DISK_BLOCK_SIZE];
                read_sd_block(block_data, indirect_block_num);

                // extra space for '\0'
                char blocknum[NUM_BYTES_PER_ADDRESS + 1];
                for (int i = 0; i <= NUM_BYTES_PER_ADDRESS; i++)
                {
                    blocknum[i] = '\0';
                }
                sprintf(blocknum, "%d", num);
                for (int i = 0; i < NUM_BYTES_PER_ADDRESS; i++)
                {
                    block_data[index * NUM_BYTES_PER_ADDRESS + i] = blocknum[i];
                }
                success = write_sd_block(block_data, indirect_block_num);
            }
        }
    }
    return success;
}

////////////// BITMAP OPERATIONS DEFINITION //////////////

// set bit k_th in bitmap.map
void set_bit(int k)
{
    int i = k / 8;
    int pos = k % 8;
    unsigned char flag = 128;
    flag = flag >> pos;
    bitmap.map[i] = bitmap.map[i] | flag;
}

// clear bit k_th in bitmap.map
void clear_bit(int k)
{
    int i = k / 8;
    int pos = k % 8;
    unsigned char flag = 128;
    flag = flag >> pos;
    flag = ~flag;
    bitmap.map[i] = bitmap.map[i] & flag;
}

int free_block(int index)
{
    if (index > bitmap.start_block && index <= bitmap.max_block)
    {
        int k = index - bitmap.start_block - 2;
        set_bit(k);
        return 1;
    }
    return 0;
}

int set_block(int index)
{
    if (index > bitmap.start_block && index <= bitmap.max_block)
    {
        int k = index - bitmap.start_block - 2;
        clear_bit(k);
        return 1;
    }
    return 0;
}

int get_free_block()
{
    const int WORD_SIZE = 8;
    int number_zero_word = 0;
    int non_zero_word = 0;

    // find non_zero_word and count zero_word
    for (int i = 0; i < bitmap.size; i++)
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

    int block_number = (WORD_SIZE * number_zero_word + offset) + bitmap.start_block + 1;
    if (block_number > 4999)
        return -1;
    else
    {
        set_block(block_number);
        return block_number;
    }
}

int write_bitmap_to_disk()
{
    int success = 0;
    for (int i = 0; i < bitmap.blocks_for_map; i++)
    {
        success = write_sd_block(bitmap.map + i * SOFTWARE_DISK_BLOCK_SIZE, (unsigned long)bitmap.start_block + i);
        if (!success)
            break;
    }
    return success;
}

int load_bitmap_from_disk()
{
    int success = 0;
    for (int i = 0; i < bitmap.blocks_for_map; i++)
    {
        success = read_sd_block(bitmap.map + i * SOFTWARE_DISK_BLOCK_SIZE, (unsigned long)bitmap.start_block + i);
        if (!success)
            break;
    }
    return success;
}

// init bitmap structure
int init_bitmap()
{
    int success = 0;
    bitmap.start_block = dir.num_blocks_for_Dir + inodes.num_blocks_for_inodes;
    bitmap.max_block = 4999;
    int num_blocks = 5000 - bitmap.start_block - 1;
    bitmap.size = (num_blocks % 8) == 0 ? num_blocks / 8 : num_blocks / 8 + 1;
    bitmap.blocks_for_map = (bitmap.size & SOFTWARE_DISK_BLOCK_SIZE) == 0 ? bitmap.size / SOFTWARE_DISK_BLOCK_SIZE : bitmap.size / SOFTWARE_DISK_BLOCK_SIZE + 1;
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
    fserror = FS_NONE;
}

File open_file(char *name, FileMode mode)
{
    if (!is_init)
    {
        is_init = 1;
        init_fs();
    }
    int f_no = get_entry(name);
    if (f_no == -1)
    {
        fserror = FS_FILE_NOT_FOUND;
        return NULL;
    }
    else
    {
        if (is_opened(f_no))
        {
            fserror = FS_FILE_OPEN;
            return NULL;
        }
        else
        {
            fserror = FS_NONE;
            File opened_file = (File)malloc(sizeof(struct FileInternals));
            opened_file->file_no = f_no;
            add_to_opened_files(f_no);
            opened_file->cur_pos = 0;
            opened_file->mode = mode;
            return opened_file;
        }
    }
}

File create_file(char *name)
{
    if (!is_init)
    {
        is_init = 1;
        init_fs();
    }
    // check filename already exsit
    int ret = get_entry(name);
    if (ret != -1)
    {
        fserror = FS_FILE_ALREADY_EXISTS;
        return NULL;
    }
    int f_no = add_entry(name);
    if (f_no != -1)
    {
        fserror = FS_NONE;
        FileInternals *created_file = malloc(sizeof(struct FileInternals));
        created_file->file_no = f_no;
        add_to_opened_files(f_no);
        created_file->cur_pos = 0;
        created_file->mode = READ_WRITE;
        return created_file;
    }
    return NULL;
}

void close_file(File file)
{
    if (!is_init)
    {
        is_init = 1;
        init_fs();
    }
    if (file != NULL)
    {
        if (is_opened(file->file_no))
        {
            delete_from_opened_files(file->file_no);
            free(file);
            fserror = FS_NONE;
        }
        else
            fserror = FS_FILE_NOT_OPEN;
    }
    else
        fserror = FS_IO_ERROR;
}

unsigned long read_file(File file, void *buf, unsigned long numbytes)
{
    if (!is_init)
    {
        is_init = 1;
        init_fs();
    }
    if (file != NULL)
    {
        if (is_opened(file->file_no))
        {
            // get the inode
            char file_inode[INODE_SIZE];
            int success = read_inode(file_inode, file->file_no);
            if (!success)
                printf("invalid file number!\n");

            // get file_size from inode
            int file_size = get_size_in_inode(file_inode);
            int eof_pos = file_size > 0 ? (file_size - 1) : 0;

            // numbytes to be read
            int numbytes_read = 0;
            if ((file->cur_pos + numbytes - 1) < file_size)
            {
                numbytes_read = numbytes;
            }
            else
            {
                numbytes_read = file_size - file->cur_pos - 1;
            }

            // calculate the number of blocks will be loaded
            int LOADED_BLOCKS = 0;
            int remain_bytes = SOFTWARE_DISK_BLOCK_SIZE - eof_pos % SOFTWARE_DISK_BLOCK_SIZE;
            if (file_size > 0)
            {
                if (numbytes_read <= remain_bytes)
                    LOADED_BLOCKS = 1;
                else
                    LOADED_BLOCKS = (numbytes_read - remain_bytes) / SOFTWARE_DISK_BLOCK_SIZE + 1;
            }
            else
                LOADED_BLOCKS = (numbytes_read - 1) / SOFTWARE_DISK_BLOCK_SIZE + 1;

            // array of block numbers for read_file
            int indexes[LOADED_BLOCKS];

            // start_block index
            int start_block = file->cur_pos / SOFTWARE_DISK_BLOCK_SIZE;
            // end_block index
            int end_block = start_block + LOADED_BLOCKS - 1;

            for (int i = start_block; i <= end_block; i++)
            {
                indexes[i - start_block] = get_block_num(file_inode, i);
            }

            // read data into buf
            char *charBuf = (char *)buf;
            int cur_pos = file->cur_pos % SOFTWARE_DISK_BLOCK_SIZE;
            if (LOADED_BLOCKS == 1)
            {
                char data[SOFTWARE_DISK_BLOCK_SIZE];
                read_sd_block(data, indexes[0]);
                for (int i = 0; i < numbytes_read; i++)
                {
                    charBuf[i] = data[cur_pos + i];
                }
                return numbytes_read;
            }
            else
            {
                // handle 1st block
                char data[SOFTWARE_DISK_BLOCK_SIZE];
                read_sd_block(data, indexes[0]);
                for (int i = 0; i < SOFTWARE_DISK_BLOCK_SIZE - cur_pos; i++)
                {
                    charBuf[i] = data[cur_pos + i];
                }

                // handle inner blocks
                int next_pos = SOFTWARE_DISK_BLOCK_SIZE - cur_pos;
                for (int i = 1; i < LOADED_BLOCKS - 1; i++)
                {
                    read_sd_block(charBuf + next_pos + (i - 1) * SOFTWARE_DISK_BLOCK_SIZE, indexes[i]);
                }

                // handle last block
                read_sd_block(data, indexes[LOADED_BLOCKS - 1]);
                int end_index = (numbytes_read - next_pos) % SOFTWARE_DISK_BLOCK_SIZE;
                for (int i = 0; i < end_index; i++)
                {
                    charBuf[next_pos + (LOADED_BLOCKS - 2) * SOFTWARE_DISK_BLOCK_SIZE + i] = data[i];
                }
                charBuf[numbytes_read] = '\0';
                return numbytes_read;
            }
        }
        else
        {
            fserror = FS_FILE_NOT_OPEN;
            return 0;
        }
    }
    else
    {
        fserror = FS_IO_ERROR;
        return 0;
    }
}

unsigned long write_file(File file, void *buf, unsigned long numbytes)
{
    if (!is_init)
    {
        is_init = 1;
        init_fs();
    }
    if (file != NULL)
    {
        if (is_opened(file->file_no))
        {
            if (file->mode == READ_WRITE)
            {
                printf("CURRENT POSITION %ld\n", file->cur_pos);
                // get file inode
                char file_inode[INODE_SIZE];
                read_inode(file_inode, file->file_no);

                // File specs
                int file_size = get_size_in_inode(file_inode);
                int eof_pos = file_size > 0 ? (file_size - 1) : 0;

                // numbytes to be written
                int numbytes_written = 0;
                if ((file->cur_pos + numbytes) < MAX_FILE_SIZE)
                {
                    fserror = FS_NONE;
                    numbytes_written = numbytes;
                }
                else
                {
                    fserror = FS_EXCEEDS_MAX_FILE_SIZE;
                    numbytes_written = MAX_FILE_SIZE - file->cur_pos - 1;
                }

                int NEEDED_BLOCKS = 0;
                int remain_bytes = SOFTWARE_DISK_BLOCK_SIZE - eof_pos % SOFTWARE_DISK_BLOCK_SIZE;
                // number of blocks needed for write
                if (file_size > 0)
                {
                    if (numbytes_written <= remain_bytes)
                        NEEDED_BLOCKS = 1;
                    else
                        NEEDED_BLOCKS = (numbytes_written - remain_bytes) / SOFTWARE_DISK_BLOCK_SIZE + 1;
                }
                else
                    NEEDED_BLOCKS = (numbytes_written - 1) / SOFTWARE_DISK_BLOCK_SIZE + 1;

                // array of block numbers for write_file
                int indexes[NEEDED_BLOCKS];

                int start_block = file->cur_pos / SOFTWARE_DISK_BLOCK_SIZE;
                int end_block = start_block + NEEDED_BLOCKS - 1;
                int eof_block = file_size > 0 ? (file_size - 1) / SOFTWARE_DISK_BLOCK_SIZE : 0;

                // current number of blocks for file
                int cur_num_blocks = get_blocks_in_inode(file_inode);

                // read block number from inode into indexes, may need to allocate new free block
                // handle empty file
                if (cur_num_blocks == 0)
                {
                    for (int i = start_block; i <= end_block; i++)
                    {
                        int new_block_num = get_free_block();
                        if (new_block_num == -1)
                        {
                            fserror = FS_OUT_OF_SPACE;
                            break;
                        }
                        if (i == 12)
                        {
                            set_direct_block_num(file_inode, i, new_block_num);
                            // get another block in put into indirect block
                            new_block_num = get_free_block();
                            if (new_block_num == -1)
                            {
                                fserror = FS_OUT_OF_SPACE;
                                break;
                            }
                        }
                        set_block_num(file_inode, i, new_block_num);
                        cur_num_blocks++;
                        indexes[i - start_block] = new_block_num;
                    }
                    set_blocks_in_inode(file_inode, cur_num_blocks);
                }
                // handle case when start in the middle of the file and write not exceed current file size
                else if (end_block <= eof_block)
                {
                    for (int i = start_block; i <= end_block; i++)
                    {
                        indexes[i - start_block] = get_block_num(file_inode, i);
                    }
                }
                // handle case when write exceed current file size
                else
                {
                    // get block number of allocated blocks
                    for (int i = start_block; i <= eof_block; i++)
                    {
                        indexes[i - start_block] = get_block_num(file_inode, i);
                    }
                    // allocate new blocks and put into inode
                    for (int i = eof_block + 1; i <= end_block; i++)
                    {
                        int new_block_num = get_free_block();
                        if (new_block_num == -1)
                        {
                            fserror = FS_OUT_OF_SPACE;
                            break;
                        }
                        // when it hits single indirect block in inode
                        if (i == 12)
                        {
                            set_direct_block_num(file_inode, i, new_block_num);
                            // get another block in put into indirect block
                            new_block_num = get_free_block();
                            if (new_block_num == -1)
                            {
                                fserror = FS_OUT_OF_SPACE;
                                break;
                            }
                        }
                        set_block_num(file_inode, i, new_block_num);
                        cur_num_blocks++;
                        indexes[i - start_block] = new_block_num;
                    }
                    set_blocks_in_inode(file_inode, cur_num_blocks);
                }

                // calculate actual need block in case disk full
                const int ACTUAL_NEEDED_BLOCKS = cur_num_blocks - file->cur_pos / SOFTWARE_DISK_BLOCK_SIZE;

                // re-calculate numbytes_written in case disk full
                if (fserror == FS_OUT_OF_SPACE)
                {
                    numbytes_written = cur_num_blocks * SOFTWARE_DISK_BLOCK_SIZE - file->cur_pos;
                }

                // write data from buf into file
                char *charBuf = (char *)buf;
                int cur_pos = file->cur_pos % SOFTWARE_DISK_BLOCK_SIZE;
                if (ACTUAL_NEEDED_BLOCKS == 1)
                {
                    // read from disk
                    char data[SOFTWARE_DISK_BLOCK_SIZE];
                    read_sd_block(data, indexes[0]);

                    // only overwrite needed bytes
                    for (int i = 0; i < numbytes_written; i++)
                    {
                        data[cur_pos + i] = charBuf[i];
                    }

                    // write to disk
                    write_sd_block(data, indexes[0]);
                }
                else
                {
                    // handle 1st block
                    char data[SOFTWARE_DISK_BLOCK_SIZE];
                    read_sd_block(data, indexes[0]);
                    for (int i = cur_pos; i < SOFTWARE_DISK_BLOCK_SIZE; i++)
                    {
                        data[i] = charBuf[i - cur_pos];
                    }
                    write_sd_block(data, indexes[0]);

                    // handle inner blocks, overwrite the whole block
                    int next_pos = SOFTWARE_DISK_BLOCK_SIZE - cur_pos;
                    for (int i = 1; i < ACTUAL_NEEDED_BLOCKS - 1; i++)
                    {
                        write_sd_block(charBuf + next_pos + (i - 1) * SOFTWARE_DISK_BLOCK_SIZE, indexes[i]);
                    }

                    // handle last block
                    read_sd_block(data, indexes[ACTUAL_NEEDED_BLOCKS - 1]);
                    int end_index = (numbytes_written - next_pos) % SOFTWARE_DISK_BLOCK_SIZE;
                    for (int i = 0; i < end_index; i++)
                    {
                        data[i] = charBuf[next_pos + (ACTUAL_NEEDED_BLOCKS - 2) * SOFTWARE_DISK_BLOCK_SIZE + i];
                    }
                    write_sd_block(data, indexes[ACTUAL_NEEDED_BLOCKS - 1]);
                }

                // update file_size
                int new_file_size = file->cur_pos + numbytes_written;
                file_size = file_size > new_file_size ? file_size : new_file_size;
                file->cur_pos = file_size;
                set_size_in_inode(file_inode, file_size);
                write_inode(file_inode, file->file_no);

                // write fs changes to disk
                write_inode_to_disk(file->file_no);
                write_bitmap_to_disk();

                return numbytes_written;
            }
            else
            {
                fserror = FS_FILE_READ_ONLY;
                return 0;
            }
        }
        else
        {
            fserror = FS_FILE_NOT_OPEN;
            return 0;
        }
    }
    else
    {
        fserror = FS_IO_ERROR;
        return 0;
    }
}

int seek_file(File file, unsigned long bytepos)
{
    if (!is_init)
    {
        is_init = 1;
        init_fs();
    }
    if (file != NULL)
    {
        if (bytepos < MAX_FILE_SIZE)
        { // get file_inode
            char file_inode[INODE_SIZE];
            read_inode(file_inode, file->file_no);

            int file_size = get_size_in_inode(file_inode);
            int eof_pos = file_size > 0 ? (file_size - 1) : 0;

            int extend_bytes = 0;
            int needed_blocks = 0;

            // HANDLE EMPTY FILE
            if (file_size == 0)
            {
                extend_bytes = bytepos;
                needed_blocks = extend_bytes / SOFTWARE_DISK_BLOCK_SIZE + 1;
            }
            // WHEN FILE NOT EMPTY
            else if (bytepos > eof_pos)
            {
                extend_bytes = bytepos - eof_pos;
                // reamaing bytes to full block from eof
                int remain_bytes = SOFTWARE_DISK_BLOCK_SIZE - eof_pos % SOFTWARE_DISK_BLOCK_SIZE;

                // needed to extend file
                needed_blocks = (extend_bytes - remain_bytes) / SOFTWARE_DISK_BLOCK_SIZE + 1;
            }
            else
            {
                fserror = FS_NONE;
                file->cur_pos = bytepos;
                return 1;
            }

            // current number of blocks for file
            int cur_num_blocks = get_blocks_in_inode(file_inode);
            int start_block = cur_num_blocks;

            // expected end_block
            int expected_end_block = start_block + needed_blocks - 1;
            for (int i = start_block; i <= expected_end_block; i++)
            {
                int block_num = get_free_block();
                if (block_num == -1)
                {
                    fserror = FS_OUT_OF_SPACE;
                    break;
                }
                if (i == 12)
                {
                    set_direct_block_num(file_inode, i, block_num);
                    // get another block in put into indirect block
                    block_num = get_free_block();
                    if (block_num == -1)
                    {
                        fserror = FS_OUT_OF_SPACE;
                        break;
                    }
                }
                set_block_num(file_inode, i, block_num);
                cur_num_blocks++;
            }
            set_blocks_in_inode(file_inode, cur_num_blocks);
            if (fserror == FS_OUT_OF_SPACE)
            {
                file_size = cur_num_blocks * SOFTWARE_DISK_BLOCK_SIZE;
            }
            else
            {
                file_size = bytepos + 1;
            }

            // seek to appropriate position
            file->cur_pos = file_size - 1;

            // update file_size
            set_size_in_inode(file_inode, file_size);
            write_inode(file_inode, file->file_no);

            // write changes to disk
            write_inode_to_disk(file->file_no);
            write_bitmap_to_disk();
            return 1;
        }
        else
        {
            fserror = FS_EXCEEDS_MAX_FILE_SIZE;
            return 0;
        }
    }
    else
    {
        fserror = FS_IO_ERROR;
        return 0;
    }
}

unsigned long file_length(File file)
{
    if (!is_init)
    {
        is_init = 1;
        init_fs();
    }
    if (file != NULL)
    {
        char file_inode[INODE_SIZE];
        int success = read_inode(file_inode, file->file_no);
        if (success)
            fserror = FS_NONE;
        else
            fserror = FS_IO_ERROR;
        return get_size_in_inode(file_inode);
    }
    else
    {
        fserror = FS_IO_ERROR;
        return 0;
    }
}

int delete_file(char *name)
{
    if (!is_init)
    {
        is_init = 1;
        init_fs();
    }
    int success = 0;
    int file_no = get_entry(name);
    if (file_no != -1)
    {
        // get file_inode
        char file_inode[INODE_SIZE];
        read_inode(file_inode, file_no);
        int num_blocks = get_blocks_in_inode(file_inode);

        // handle empty file
        if (num_blocks == 0)
        {
            delete_entry(file_no);
        }
        else
        {
            char empty_data[SOFTWARE_DISK_BLOCK_SIZE];
            for (int i = 0; i < SOFTWARE_DISK_BLOCK_SIZE; i++)
            {
                empty_data[i] = '\0';
            }

            // wipe out data and free the blocks
            for (int i = 0; i < num_blocks; i++)
            {
                int block_num = get_block_num(file_inode, i);
                free_block(block_num);
                write_sd_block(empty_data, block_num);
            }
            delete_entry(file_no);

            // write changes to disk
            write_bitmap_to_disk();
        }

        success = 1;
    }
    else
    {
        fserror = FS_FILE_NOT_FOUND;
    }
    return success;
}

int file_exists(char *name)
{
    if (!is_init)
    {
        is_init = 1;
        init_fs();
    }
    int ret = get_entry(name);
    fserror = FS_NONE;
    if (ret == -1)
        return 0;
    else
        return 1;
}

void fs_print_error(void)
{
    if (!is_init)
    {
        is_init = 1;
        init_fs();
    }
    switch (fserror)
    {
    case FS_NONE:
        printf("Filesystem is working properly.\n");
        break;
    case FS_OUT_OF_SPACE:
        printf("The operation caused the software disk to fill up\n");
        break;
    case FS_FILE_NOT_OPEN:
        printf("attempted read/write/close/etc. on file that isn't open.\n");
        break;
    case FS_FILE_OPEN:
        printf("file is already open. Concurrent opens are not supported and neither is deleting a file that is open.\n");
        break;
    case FS_FILE_NOT_FOUND:
        printf("attempted open or delete of file that doesn't exist.\n");
        break;
    case FS_FILE_READ_ONLY:
        printf("attempted write to file opened for READ_ONLY.\n");
        break;
    case FS_FILE_ALREADY_EXISTS:
        printf("attempted creation of file with existing name.\n");
        break;
    case FS_EXCEEDS_MAX_FILE_SIZE:
        printf("seek or write would exceed max file size.\n");
        break;
    case FS_ILLEGAL_FILENAME:
        printf("filename begins with a null character\n");
        break;
    default:
        printf("something really bad happened.\n");
    }
}