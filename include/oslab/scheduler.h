#ifndef OSLAB_SCHEDULER_H
#define OSLAB_SCHEDULER_H

#define OSLAB_MAX_PROCESSES 64
#define OSLAB_MAX_SEGMENTS 512

typedef struct {
    int pid;
    int arrival_time;
    int burst_time;
    int priority;
} Process;

typedef struct {
    int pid;
    int start_time;
    int end_time;
} ScheduleSegment;

typedef struct {
    int pid;
    int completion_time;
    int turnaround_time;
    int waiting_time;
    int response_time;
} ProcessResult;

typedef struct {
    ScheduleSegment segments[OSLAB_MAX_SEGMENTS];
    int segment_count;
    ProcessResult results[OSLAB_MAX_PROCESSES];
    int process_count;
    double average_turnaround_time;
    double average_waiting_time;
    double average_response_time;
} ScheduleReport;

int schedule_fcfs(const Process *processes, int count, ScheduleReport *report);
int schedule_sjf(const Process *processes, int count, ScheduleReport *report);
int schedule_priority(const Process *processes, int count, ScheduleReport *report);
int schedule_rr(const Process *processes, int count, int quantum, ScheduleReport *report);

void schedule_print_report(const ScheduleReport *report);

#endif
