# A SIMPLE FILESYSTEM
## Description
Implementation of a simple filesystem from scratch with the provided implementation of a persistent "raw" software disk (available in softwaredisk.h and softwaredisk.c), which supports reads and writes of fixed-sized blocks (numbered 0..software_disk_size() - 1).
The goal is to wrap a higher-level filesystem interface around the provided software disk implementation by implementing an API that tracks files that are created, allocates blocks for file allocation, the directory structure, and file data.
## Limitation
Max numbers of file supported: 800
Max size in bytes per file is 71679
Max name length for a file is 60
