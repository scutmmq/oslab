#ifndef OSLAB_FILESYSTEM_H
#define OSLAB_FILESYSTEM_H

#include <stdbool.h>
#include <stddef.h>

#define FS_BLOCK_SIZE 64
#define FS_BLOCK_COUNT 128
#define FS_MAX_FILES 32
#define FS_MAX_FILENAME 32
#define FS_MAX_FILE_BLOCKS 16

typedef struct {
    char name[FS_MAX_FILENAME];
    int size;
    int block_count;
    int blocks[FS_MAX_FILE_BLOCKS];
    bool used;
} DirectoryEntry;

typedef struct {
    DirectoryEntry entries[FS_MAX_FILES];
    unsigned char blocks[FS_BLOCK_COUNT][FS_BLOCK_SIZE];
    bool free_map[FS_BLOCK_COUNT];
} SimpleFS;

void fs_format(SimpleFS *fs);
int fs_create(SimpleFS *fs, const char *name);
int fs_write(SimpleFS *fs, const char *name, const char *content);
int fs_read(const SimpleFS *fs, const char *name, char *buffer, size_t buffer_size);
int fs_delete(SimpleFS *fs, const char *name);
int fs_free_block_count(const SimpleFS *fs);
void fs_list(const SimpleFS *fs);

#endif
