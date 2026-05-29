#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "oslab/filesystem.h"
#include "oslab/memory.h"
#include "oslab/scheduler.h"

static const ProcessResult *find_result(const ScheduleReport *report, int pid) {
    for (int i = 0; i < report->process_count; ++i) {
        if (report->results[i].pid == pid) {
            return &report->results[i];
        }
    }
    return NULL;
}

static void test_fcfs_and_sjf_scheduling(void) {
    const Process processes[] = {
        {.pid = 1, .arrival_time = 0, .burst_time = 4, .priority = 2},
        {.pid = 2, .arrival_time = 1, .burst_time = 3, .priority = 1},
        {.pid = 3, .arrival_time = 2, .burst_time = 1, .priority = 3},
    };
    ScheduleReport report;

    assert(schedule_fcfs(processes, 3, &report) == 0);
    assert(report.segment_count == 3);
    assert(report.segments[0].pid == 1);
    assert(report.segments[1].pid == 2);
    assert(report.segments[2].pid == 3);
    assert(find_result(&report, 1)->completion_time == 4);
    assert(find_result(&report, 2)->turnaround_time == 6);
    assert(find_result(&report, 3)->waiting_time == 5);

    assert(schedule_sjf(processes, 3, &report) == 0);
    assert(report.segment_count == 3);
    assert(report.segments[0].pid == 1);
    assert(report.segments[1].pid == 3);
    assert(report.segments[2].pid == 2);
    assert(find_result(&report, 3)->completion_time == 5);
    assert(find_result(&report, 2)->waiting_time == 4);
}

static void test_round_robin_scheduling(void) {
    const Process processes[] = {
        {.pid = 1, .arrival_time = 0, .burst_time = 5, .priority = 1},
        {.pid = 2, .arrival_time = 1, .burst_time = 3, .priority = 1},
        {.pid = 3, .arrival_time = 2, .burst_time = 1, .priority = 1},
    };
    ScheduleReport report;

    assert(schedule_rr(processes, 3, 2, &report) == 0);
    assert(report.segment_count == 6);
    assert(report.segments[0].pid == 1);
    assert(report.segments[1].pid == 2);
    assert(report.segments[2].pid == 3);
    assert(report.segments[3].pid == 1);
    assert(report.segments[4].pid == 2);
    assert(report.segments[5].pid == 1);
    assert(find_result(&report, 1)->completion_time == 9);
    assert(find_result(&report, 2)->completion_time == 8);
    assert(find_result(&report, 3)->turnaround_time == 3);
}

static void test_page_replacement(void) {
    const int refs[] = {1, 2, 3, 2, 4, 1, 2};
    PageReport fifo;
    PageReport lru;

    assert(page_fifo(refs, 7, 3, &fifo) == 0);
    assert(page_lru(refs, 7, 3, &lru) == 0);
    assert(fifo.faults == 6);
    assert(fifo.hits == 1);
    assert(lru.faults == 5);
    assert(lru.hits == 2);
}

static void test_dynamic_partitions(void) {
    PartitionTable table;

    partition_init(&table, 100);
    assert(partition_allocate_first_fit(&table, 1, 20) == 0);
    assert(partition_allocate_first_fit(&table, 2, 30) == 20);
    assert(partition_free(&table, 1) == 0);
    assert(partition_allocate_first_fit(&table, 3, 10) == 0);
    assert(partition_free(&table, 2) == 0);
    assert(partition_free(&table, 3) == 0);
    assert(table.count == 1);
    assert(table.parts[0].free);
    assert(table.parts[0].size == 100);

    partition_init(&table, 120);
    assert(partition_allocate_first_fit(&table, 1, 20) == 0);
    assert(partition_allocate_first_fit(&table, 2, 50) == 20);
    assert(partition_allocate_first_fit(&table, 3, 20) == 70);
    assert(partition_free(&table, 1) == 0);
    assert(partition_free(&table, 3) == 0);
    assert(partition_allocate_best_fit(&table, 4, 15) == 0);
}

static void test_simple_file_system(void) {
    SimpleFS fs;
    char buffer[128];
    char huge[FS_BLOCK_SIZE * FS_MAX_FILE_BLOCKS + 2];

    fs_format(&fs);
    const int initial_free = fs_free_block_count(&fs);
    assert(fs_create(&fs, "note.txt") == 0);
    assert(fs_write(&fs, "note.txt", "hello operating system") == 0);
    memset(buffer, 0, sizeof(buffer));
    assert(fs_read(&fs, "note.txt", buffer, sizeof(buffer)) == 22);
    assert(strcmp(buffer, "hello operating system") == 0);
    assert(fs_free_block_count(&fs) == initial_free - 1);
    assert(fs_delete(&fs, "note.txt") == 0);
    assert(fs_free_block_count(&fs) == initial_free);
    assert(fs_read(&fs, "note.txt", buffer, sizeof(buffer)) < 0);

    assert(fs_create(&fs, "stable.txt") == 0);
    assert(fs_write(&fs, "stable.txt", "keep me") == 0);
    memset(huge, 'x', sizeof(huge) - 1);
    huge[sizeof(huge) - 1] = '\0';
    assert(fs_write(&fs, "stable.txt", huge) < 0);
    memset(buffer, 0, sizeof(buffer));
    assert(fs_read(&fs, "stable.txt", buffer, sizeof(buffer)) == 7);
    assert(strcmp(buffer, "keep me") == 0);
}

int main(void) {
    test_fcfs_and_sjf_scheduling();
    test_round_robin_scheduling();
    test_page_replacement();
    test_dynamic_partitions();
    test_simple_file_system();
    puts("All core tests passed.");
    return 0;
}
