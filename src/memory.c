#include "oslab/memory.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

static int validate_page_input(const int *references, int reference_count, int frame_count, PageReport *report) {
    if (references == NULL || report == NULL || reference_count <= 0 ||
        reference_count > OSLAB_MAX_REFERENCES || frame_count <= 0 || frame_count > OSLAB_MAX_FRAMES) {
        return -1;
    }
    memset(report, 0, sizeof(*report));
    report->frame_count = frame_count;
    report->reference_count = reference_count;
    return 0;
}

static int find_page(const int *frames, int frame_count, int page) {
    for (int i = 0; i < frame_count; ++i) {
        if (frames[i] == page) {
            return i;
        }
    }
    return -1;
}

static void record_page_step(PageReport *report, int step, int page, const int *frames, bool fault, int evicted) {
    report->steps[step].page = page;
    report->steps[step].fault = fault;
    report->steps[step].evicted_page = evicted;
    for (int i = 0; i < report->frame_count; ++i) {
        report->steps[step].frames[i] = frames[i];
    }
}

int page_fifo(const int *references, int reference_count, int frame_count, PageReport *report) {
    if (validate_page_input(references, reference_count, frame_count, report) != 0) {
        return -1;
    }

    int frames[OSLAB_MAX_FRAMES];
    int loaded = 0;
    int next_replace = 0;
    for (int i = 0; i < frame_count; ++i) {
        frames[i] = -1;
    }

    for (int i = 0; i < reference_count; ++i) {
        const int page = references[i];
        const int hit_slot = find_page(frames, frame_count, page);
        int evicted = -1;
        if (hit_slot >= 0) {
            ++report->hits;
            record_page_step(report, i, page, frames, false, evicted);
            continue;
        }

        ++report->faults;
        if (loaded < frame_count) {
            frames[loaded++] = page;
            if (loaded == frame_count) {
                next_replace = 0;
            }
        } else {
            evicted = frames[next_replace];
            frames[next_replace] = page;
            next_replace = (next_replace + 1) % frame_count;
        }
        record_page_step(report, i, page, frames, true, evicted);
    }

    report->fault_rate = (double)report->faults / reference_count;
    return 0;
}

int page_lru(const int *references, int reference_count, int frame_count, PageReport *report) {
    if (validate_page_input(references, reference_count, frame_count, report) != 0) {
        return -1;
    }

    int frames[OSLAB_MAX_FRAMES];
    int last_used[OSLAB_MAX_FRAMES];
    for (int i = 0; i < frame_count; ++i) {
        frames[i] = -1;
        last_used[i] = -1;
    }

    for (int i = 0; i < reference_count; ++i) {
        const int page = references[i];
        const int hit_slot = find_page(frames, frame_count, page);
        int evicted = -1;
        if (hit_slot >= 0) {
            ++report->hits;
            last_used[hit_slot] = i;
            record_page_step(report, i, page, frames, false, evicted);
            continue;
        }

        int slot = find_page(frames, frame_count, -1);
        if (slot < 0) {
            int oldest = INT_MAX;
            for (int j = 0; j < frame_count; ++j) {
                if (last_used[j] < oldest) {
                    oldest = last_used[j];
                    slot = j;
                }
            }
            evicted = frames[slot];
        }
        frames[slot] = page;
        last_used[slot] = i;
        ++report->faults;
        record_page_step(report, i, page, frames, true, evicted);
    }

    report->fault_rate = (double)report->faults / reference_count;
    return 0;
}

void page_print_report(const PageReport *report) {
    if (report == NULL) {
        return;
    }

    puts("\n访问\t内存块\t\t结果");
    for (int i = 0; i < report->reference_count; ++i) {
        printf("%d\t", report->steps[i].page);
        for (int j = 0; j < report->frame_count; ++j) {
            if (report->steps[i].frames[j] < 0) {
                printf("- ");
            } else {
                printf("%d ", report->steps[i].frames[j]);
            }
        }
        printf("\t%s", report->steps[i].fault ? "缺页" : "命中");
        if (report->steps[i].evicted_page >= 0) {
            printf("(淘汰 %d)", report->steps[i].evicted_page);
        }
        putchar('\n');
    }
    printf("缺页次数: %d\n", report->faults);
    printf("命中次数: %d\n", report->hits);
    printf("缺页率: %.2f%%\n", report->fault_rate * 100.0);
}

void partition_init(PartitionTable *table, int total_size) {
    if (table == NULL) {
        return;
    }
    memset(table, 0, sizeof(*table));
    table->total_size = total_size > 0 ? total_size : 0;
    table->count = table->total_size > 0 ? 1 : 0;
    if (table->count == 1) {
        table->parts[0] = (Partition){
            .start = 0,
            .size = table->total_size,
            .pid = -1,
            .free = true,
        };
    }
}

static int pid_exists(const PartitionTable *table, int pid) {
    for (int i = 0; i < table->count; ++i) {
        if (!table->parts[i].free && table->parts[i].pid == pid) {
            return 1;
        }
    }
    return 0;
}

static int allocate_at(PartitionTable *table, int index, int pid, int size) {
    Partition *part = &table->parts[index];
    const int start = part->start;
    if (part->size == size) {
        part->pid = pid;
        part->free = false;
        return start;
    }

    if (table->count >= OSLAB_MAX_PARTITIONS) {
        return -1;
    }

    for (int i = table->count; i > index + 1; --i) {
        table->parts[i] = table->parts[i - 1];
    }
    table->parts[index + 1] = (Partition){
        .start = part->start + size,
        .size = part->size - size,
        .pid = -1,
        .free = true,
    };
    part->size = size;
    part->pid = pid;
    part->free = false;
    ++table->count;
    return start;
}

int partition_allocate_first_fit(PartitionTable *table, int pid, int size) {
    if (table == NULL || pid <= 0 || size <= 0 || pid_exists(table, pid)) {
        return -1;
    }
    for (int i = 0; i < table->count; ++i) {
        if (table->parts[i].free && table->parts[i].size >= size) {
            return allocate_at(table, i, pid, size);
        }
    }
    return -1;
}

int partition_allocate_best_fit(PartitionTable *table, int pid, int size) {
    if (table == NULL || pid <= 0 || size <= 0 || pid_exists(table, pid)) {
        return -1;
    }
    int best = -1;
    for (int i = 0; i < table->count; ++i) {
        if (!table->parts[i].free || table->parts[i].size < size) {
            continue;
        }
        if (best < 0 || table->parts[i].size < table->parts[best].size) {
            best = i;
        }
    }
    return best < 0 ? -1 : allocate_at(table, best, pid, size);
}

static void merge_free_neighbors(PartitionTable *table) {
    int i = 0;
    while (i < table->count - 1) {
        if (table->parts[i].free && table->parts[i + 1].free) {
            table->parts[i].size += table->parts[i + 1].size;
            for (int j = i + 1; j < table->count - 1; ++j) {
                table->parts[j] = table->parts[j + 1];
            }
            --table->count;
        } else {
            ++i;
        }
    }
}

int partition_free(PartitionTable *table, int pid) {
    if (table == NULL || pid <= 0) {
        return -1;
    }
    for (int i = 0; i < table->count; ++i) {
        if (!table->parts[i].free && table->parts[i].pid == pid) {
            table->parts[i].free = true;
            table->parts[i].pid = -1;
            merge_free_neighbors(table);
            return 0;
        }
    }
    return -1;
}

void partition_print(const PartitionTable *table) {
    if (table == NULL) {
        return;
    }
    puts("\n起址\t大小\t状态");
    for (int i = 0; i < table->count; ++i) {
        printf("%d\t%d\t", table->parts[i].start, table->parts[i].size);
        if (table->parts[i].free) {
            puts("空闲");
        } else {
            printf("P%d\n", table->parts[i].pid);
        }
    }
}
