#ifndef OSLAB_MEMORY_H
#define OSLAB_MEMORY_H

#include <stdbool.h>

#define OSLAB_MAX_FRAMES 16
#define OSLAB_MAX_REFERENCES 256
#define OSLAB_MAX_PARTITIONS 64

typedef struct {
    int page;
    int frames[OSLAB_MAX_FRAMES];
    int evicted_page;
    bool fault;
} PageStep;

typedef struct {
    int frame_count;
    int reference_count;
    int faults;
    int hits;
    double fault_rate;
    PageStep steps[OSLAB_MAX_REFERENCES];
} PageReport;

typedef struct {
    int start;
    int size;
    int pid;
    bool free;
} Partition;

typedef struct {
    Partition parts[OSLAB_MAX_PARTITIONS];
    int count;
    int total_size;
} PartitionTable;

int page_fifo(const int *references, int reference_count, int frame_count, PageReport *report);
int page_lru(const int *references, int reference_count, int frame_count, PageReport *report);
void page_print_report(const PageReport *report);

void partition_init(PartitionTable *table, int total_size);
int partition_allocate_first_fit(PartitionTable *table, int pid, int size);
int partition_allocate_best_fit(PartitionTable *table, int pid, int size);
int partition_free(PartitionTable *table, int pid);
void partition_print(const PartitionTable *table);

#endif
