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
    unsigned long eof_pos; // COULD BE IN INODES
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

// delete an entry at index
int delete_entry(int index);

// add an new entry with given filename, return the index of new entry, return -1  max file number exceeded or illegal filename
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

// delete an inode at index, return 1 for success, 0 for error
int delete_inode(int index);

// add an empty inode at index, return 1 for success, 0 for error
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
int get_block_num(char *inode_data, int direct_block);

// set the block number at specified direct block with given inode, return 1 for success, 0 for error
int set_block_num(char *inode_data, int direct_block, int num);

// return the block number with given block and index(0-127: use in single indirect block), return -1 for error
int get_block_num_from_block(char *block_data, int index);

// set the block number at specified index(0-127: use in single indirect block) with given block, return 1 for success, 0 for error
int set_block_num_to_block(char *block_data, int index, int num);

typedef struct BitMap
{
    int start_block;
    int max_block;
    int size;
    char map[SOFTWARE_DISK_BLOCK_SIZE];
} BitMap;

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
                inodes.list_inodes[index][0] = '0';
                inodes.list_inodes[index][1] = '0';
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

int get_block_num(char *inode_data, int direct_block)
{
    int blocknum = -1;
    if (direct_block >= 0 && direct_block <= NUM_DIRECT_BLOCK)
    {
        char num[NUM_BYTES_PER_ADDRESS];
        for (int i = 0; i < NUM_BYTES_PER_ADDRESS; i++)
        {
            num[i] = inode_data[NUM_BYTES_FOR_SIZE + NUM_BYTES_FOR_BLOCKS + direct_block * NUM_BYTES_PER_ADDRESS];
        }
        blocknum = atoi(num);
    }
    return blocknum;
}

int set_block_num(char *inode_data, int direct_block, int num)
{
    int success = 0;
    if (direct_block >= 0 && direct_block < NUM_DIRECT_BLOCK)
    {
        if (num > bitmap.start_block && num <= bitmap.max_block)
        {
            char blocknum[NUM_BYTES_PER_ADDRESS];
            for (int i = 0; i <= NUM_BYTES_PER_ADDRESS; i++)
            {
                blocknum[i] = '\0';
            }
            sprintf(blocknum, "%d", num);
            for (int i = 0; i <= NUM_BYTES_PER_ADDRESS; i++)
            {
                inode_data[NUM_BYTES_FOR_SIZE + NUM_BYTES_FOR_BLOCKS + direct_block * NUM_BYTES_PER_ADDRESS] = blocknum[i];
            }
            success = 1;
        }
    }
    return success;
}

int get_block_num_from_block(char *block_data, int index)
{
    int blocknum = -1;
    const int NUM_ADDRESS_PER_BLOCK = SOFTWARE_DISK_BLOCK_SIZE / NUM_BYTES_PER_ADDRESS;
    if (index >= 0 && index < NUM_ADDRESS_PER_BLOCK)
    {
        char num[NUM_BYTES_PER_ADDRESS];
        for (int i = 0; i < NUM_BYTES_PER_ADDRESS; i++)
        {
            num[i] = block_data[index * NUM_BYTES_PER_ADDRESS + i];
        }
        blocknum = atoi(num);
    }
    return blocknum;
}

int set_block_num_to_block(char *block_data, int index, int num)
{
    int success = 0;
    const int NUM_ADDRESS_PER_BLOCK = SOFTWARE_DISK_BLOCK_SIZE / NUM_BYTES_PER_ADDRESS;
    if (index >= 0 && index < NUM_ADDRESS_PER_BLOCK)
    {
        if (num > bitmap.start_block && num <= bitmap.max_block)
        {
            char blocknum[NUM_BYTES_PER_ADDRESS];
            for (int i = 0; i <= NUM_BYTES_PER_ADDRESS; i++)
            {
                blocknum[i] = '\0';
            }
            sprintf(blocknum, "%d", num);
            for (int i = 0; i <= NUM_BYTES_PER_ADDRESS; i++)
            {
                block_data[index * NUM_BYTES_PER_ADDRESS + i] = blocknum[i];
            }
            success = 1;
        }
    }
    return success;
}

////////////// BITMAP OPERATIONS DEFINITION //////////////

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

    int block_number = (WORD_SIZE * number_zero_word + offset) + bitmap.start_block - 1;
    if (block_number > 4999)
        return -1;
    else
        return block_number;
}

int write_bitmap_to_disk()
{
    return write_sd_block(bitmap.map, (unsigned long)bitmap.start_block);
}

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

File open_file(char *name, FileMode mode)
{
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
            FileInternals *opened_file = malloc(sizeof(struct FileInternals));
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
    // printf("inside close_file\n");
    // printf("file_no is %ld\n", file->file_no);
    if (is_opened(file->file_no))
    {
        delete_from_opened_files(file->file_no);
        free(file);
        fserror = FS_NONE;
    }
    else
        fserror = FS_FILE_NOT_OPEN;
}

unsigned long read_file(File file, void *buf, unsigned long numbytes)
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

        // calculate End Of Read (eor) with given numbytes
        int eor_pos = file->cur_pos + numbytes - 1;

        // calculate EOF
        int eof_pos = file_size - 1;

        // calculate the final end position for read_file
        int end_pos = eor_pos < eof_pos ? eor_pos : eof_pos;

        // numnber of bytes read
        const int num_bytes_read = end_pos - file->cur_pos + 1;

        // calculate the number of blocks will be loaded
        const int LOADED_BLOCKS = (num_bytes_read - 1) / SOFTWARE_DISK_BLOCK_SIZE + 1;

        // array of block numbers for read_file
        int indexes[LOADED_BLOCKS];

        if (LOADED_BLOCKS <= NUM_DIRECT_BLOCK)
        {
            for (int i = 0; i < LOADED_BLOCKS; i++)
            {
                indexes[i] = get_block_num(file_inode, i);
            }
        }
        else
        {
            for (int i = 0; i < NUM_DIRECT_BLOCK; i++)
            {
                indexes[i] = get_block_num(file_inode, i);
            }

            // number of blocks left in single indirect
            int blocks_left = LOADED_BLOCKS - NUM_DIRECT_BLOCK;

            int indirect_block = get_block_num(file_inode, 12);
            char block_data[SOFTWARE_DISK_BLOCK_SIZE];
            read_sd_block(block_data, indirect_block);
            for (int i = 0; i < blocks_left; i++)
            {
                indexes[i + NUM_DIRECT_BLOCK] = get_block_num_from_block(block_data, i);
            }
        }

        // read data into buf
        char *charBuf = (char *)buf;
        if (LOADED_BLOCKS == 1)
        {
            char data[SOFTWARE_DISK_BLOCK_SIZE];
            read_sd_block(data, indexes[0]);
            for (int i = 0; i < num_bytes_read; i++)
            {
                charBuf[i] = data[i];
            }
            return num_bytes_read;
        }
        else
        {
            for (int i = 0; i < LOADED_BLOCKS - 1; i++)
            {
                read_sd_block(charBuf + i * SOFTWARE_DISK_BLOCK_SIZE, indexes[i]);
            }

            // read the last block
            char data[SOFTWARE_DISK_BLOCK_SIZE];
            read_sd_block(data, indexes[LOADED_BLOCKS - 1]);
            int end_index = end_pos % SOFTWARE_DISK_BLOCK_SIZE;
            for (int i = 0; i <= end_index; i++)
            {
                charBuf[(LOADED_BLOCKS - 1) * SOFTWARE_DISK_BLOCK_SIZE + i] = data[i];
            }
            return num_bytes_read;
        }
    }
    else
    {
        fserror = FS_FILE_NOT_OPEN;
        return 0;
    }
}

void fs_print_error(void)
{
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