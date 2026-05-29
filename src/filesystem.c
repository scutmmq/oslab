#include "oslab/filesystem.h"

#include <stdio.h>
#include <string.h>

static int find_entry(const SimpleFS *fs, const char *name) {
    if (fs == NULL || name == NULL || name[0] == '\0') {
        return -1;
    }
    for (int i = 0; i < FS_MAX_FILES; ++i) {
        if (fs->entries[i].used && strcmp(fs->entries[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static int find_free_entry(const SimpleFS *fs) {
    for (int i = 0; i < FS_MAX_FILES; ++i) {
        if (!fs->entries[i].used) {
            return i;
        }
    }
    return -1;
}

void fs_format(SimpleFS *fs) {
    if (fs == NULL) {
        return;
    }
    memset(fs, 0, sizeof(*fs));
    for (int i = 0; i < FS_BLOCK_COUNT; ++i) {
        fs->free_map[i] = true;
    }
}

int fs_create(SimpleFS *fs, const char *name) {
    if (fs == NULL || name == NULL || name[0] == '\0' || strlen(name) >= FS_MAX_FILENAME) {
        return -1;
    }
    if (find_entry(fs, name) >= 0) {
        return -1;
    }
    const int slot = find_free_entry(fs);
    if (slot < 0) {
        return -1;
    }
    memset(&fs->entries[slot], 0, sizeof(fs->entries[slot]));
    strcpy(fs->entries[slot].name, name);
    fs->entries[slot].used = true;
    return 0;
}

static void release_blocks(SimpleFS *fs, DirectoryEntry *entry) {
    for (int i = 0; i < entry->block_count; ++i) {
        const int block = entry->blocks[i];
        if (block >= 0 && block < FS_BLOCK_COUNT) {
            fs->free_map[block] = true;
            memset(fs->blocks[block], 0, FS_BLOCK_SIZE);
        }
        entry->blocks[i] = -1;
    }
    entry->block_count = 0;
    entry->size = 0;
}

static int reserve_blocks(SimpleFS *fs, DirectoryEntry *entry, int needed) {
    if (needed > FS_MAX_FILE_BLOCKS || needed > fs_free_block_count(fs)) {
        return -1;
    }
    for (int i = 0; i < needed; ++i) {
        int selected = -1;
        for (int block = 0; block < FS_BLOCK_COUNT; ++block) {
            if (fs->free_map[block]) {
                selected = block;
                break;
            }
        }
        if (selected < 0) {
            return -1;
        }
        fs->free_map[selected] = false;
        entry->blocks[entry->block_count++] = selected;
    }
    return 0;
}

int fs_write(SimpleFS *fs, const char *name, const char *content) {
    if (fs == NULL || name == NULL || content == NULL) {
        return -1;
    }
    const int slot = find_entry(fs, name);
    if (slot < 0) {
        return -1;
    }
    DirectoryEntry *entry = &fs->entries[slot];
    const int length = (int)strlen(content);
    const int needed = length == 0 ? 0 : (length + FS_BLOCK_SIZE - 1) / FS_BLOCK_SIZE;
    if (needed > FS_MAX_FILE_BLOCKS || needed > fs_free_block_count(fs) + entry->block_count) {
        return -1;
    }

    release_blocks(fs, entry);
    if (reserve_blocks(fs, entry, needed) != 0) {
        return -1;
    }

    entry->size = length;
    int copied = 0;
    for (int i = 0; i < entry->block_count; ++i) {
        const int block = entry->blocks[i];
        const int remaining = length - copied;
        const int chunk = remaining > FS_BLOCK_SIZE ? FS_BLOCK_SIZE : remaining;
        memcpy(fs->blocks[block], content + copied, (size_t)chunk);
        copied += chunk;
    }
    return 0;
}

int fs_read(const SimpleFS *fs, const char *name, char *buffer, size_t buffer_size) {
    if (fs == NULL || name == NULL || buffer == NULL || buffer_size == 0) {
        return -1;
    }
    const int slot = find_entry(fs, name);
    if (slot < 0) {
        return -1;
    }
    const DirectoryEntry *entry = &fs->entries[slot];
    const int readable = entry->size < (int)buffer_size - 1 ? entry->size : (int)buffer_size - 1;
    int copied = 0;
    for (int i = 0; i < entry->block_count && copied < readable; ++i) {
        const int remaining = readable - copied;
        const int chunk = remaining > FS_BLOCK_SIZE ? FS_BLOCK_SIZE : remaining;
        memcpy(buffer + copied, fs->blocks[entry->blocks[i]], (size_t)chunk);
        copied += chunk;
    }
    buffer[copied] = '\0';
    return copied;
}

int fs_delete(SimpleFS *fs, const char *name) {
    if (fs == NULL || name == NULL) {
        return -1;
    }
    const int slot = find_entry(fs, name);
    if (slot < 0) {
        return -1;
    }
    release_blocks(fs, &fs->entries[slot]);
    memset(&fs->entries[slot], 0, sizeof(fs->entries[slot]));
    return 0;
}

int fs_free_block_count(const SimpleFS *fs) {
    if (fs == NULL) {
        return 0;
    }
    int count = 0;
    for (int i = 0; i < FS_BLOCK_COUNT; ++i) {
        if (fs->free_map[i]) {
            ++count;
        }
    }
    return count;
}

void fs_list(const SimpleFS *fs) {
    if (fs == NULL) {
        return;
    }
    puts("\n文件名\t大小\t块号");
    for (int i = 0; i < FS_MAX_FILES; ++i) {
        if (!fs->entries[i].used) {
            continue;
        }
        printf("%s\t%d\t", fs->entries[i].name, fs->entries[i].size);
        for (int j = 0; j < fs->entries[i].block_count; ++j) {
            printf("%d%s", fs->entries[i].blocks[j], j + 1 == fs->entries[i].block_count ? "" : ",");
        }
        putchar('\n');
    }
    printf("空闲块: %d/%d\n", fs_free_block_count(fs), FS_BLOCK_COUNT);
}
