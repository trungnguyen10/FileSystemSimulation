#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "softwaredisk.h"
#include "filesystem.h"

int main(int argc, char *argv[])
{
    init_fs();

    print_specs();

    add_entry("MyNewFile");
    add_entry("anotherfile");
    add_entry("oneMoreFile");

    for (int i = 0; i < num_used_entries(); i++)
    {
        write_entry_to_disk(i);
        write_inode_to_disk(i);
    }

    load_dir_from_disk();
    load_inodes_from_disk();

    print_entry(1);
    print_inode(1);

    return 0;
}