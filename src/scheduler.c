#include "oslab/scheduler.h"

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

typedef int (*ReadyScore)(const Process *process);

static int validate_input(const Process *processes, int count, ScheduleReport *report) {
    if (processes == NULL || report == NULL || count <= 0 || count > OSLAB_MAX_PROCESSES) {
        return -1;
    }
    for (int i = 0; i < count; ++i) {
        if (processes[i].pid <= 0 || processes[i].arrival_time < 0 || processes[i].burst_time <= 0) {
            return -1;
        }
    }
    memset(report, 0, sizeof(*report));
    report->process_count = count;
    return 0;
}

static void add_segment(ScheduleReport *report, int pid, int start_time, int end_time) {
    if (start_time == end_time || report->segment_count >= OSLAB_MAX_SEGMENTS) {
        return;
    }
    if (report->segment_count > 0) {
        ScheduleSegment *previous = &report->segments[report->segment_count - 1];
        if (previous->pid == pid && previous->end_time == start_time) {
            previous->end_time = end_time;
            return;
        }
    }
    report->segments[report->segment_count++] = (ScheduleSegment){
        .pid = pid,
        .start_time = start_time,
        .end_time = end_time,
    };
}

static void set_result(ScheduleReport *report, int slot, const Process *process, int first_start, int completion) {
    const int turnaround = completion - process->arrival_time;
    report->results[slot] = (ProcessResult){
        .pid = process->pid,
        .completion_time = completion,
        .turnaround_time = turnaround,
        .waiting_time = turnaround - process->burst_time,
        .response_time = first_start - process->arrival_time,
    };
}

static void compute_averages(ScheduleReport *report) {
    double turnaround = 0.0;
    double waiting = 0.0;
    double response = 0.0;
    for (int i = 0; i < report->process_count; ++i) {
        turnaround += report->results[i].turnaround_time;
        waiting += report->results[i].waiting_time;
        response += report->results[i].response_time;
    }
    report->average_turnaround_time = turnaround / report->process_count;
    report->average_waiting_time = waiting / report->process_count;
    report->average_response_time = response / report->process_count;
}

static int earlier_process(const Process *a, const Process *b) {
    if (a->arrival_time != b->arrival_time) {
        return a->arrival_time < b->arrival_time;
    }
    return a->pid < b->pid;
}

int schedule_fcfs(const Process *processes, int count, ScheduleReport *report) {
    if (validate_input(processes, count, report) != 0) {
        return -1;
    }

    bool done[OSLAB_MAX_PROCESSES] = {false};
    int time = 0;
    for (int completed = 0; completed < count; ++completed) {
        int selected = -1;
        for (int i = 0; i < count; ++i) {
            if (!done[i] && (selected < 0 || earlier_process(&processes[i], &processes[selected]))) {
                selected = i;
            }
        }
        if (time < processes[selected].arrival_time) {
            time = processes[selected].arrival_time;
        }
        const int start = time;
        time += processes[selected].burst_time;
        add_segment(report, processes[selected].pid, start, time);
        set_result(report, selected, &processes[selected], start, time);
        done[selected] = true;
    }

    compute_averages(report);
    return 0;
}

static int score_burst(const Process *process) {
    return process->burst_time;
}

static int score_priority(const Process *process) {
    return process->priority;
}

static int schedule_non_preemptive(
    const Process *processes,
    int count,
    ScheduleReport *report,
    ReadyScore score
) {
    if (validate_input(processes, count, report) != 0) {
        return -1;
    }

    bool done[OSLAB_MAX_PROCESSES] = {false};
    int time = 0;
    int completed = 0;
    while (completed < count) {
        int selected = -1;
        for (int i = 0; i < count; ++i) {
            if (done[i] || processes[i].arrival_time > time) {
                continue;
            }
            if (selected < 0 ||
                score(&processes[i]) < score(&processes[selected]) ||
                (score(&processes[i]) == score(&processes[selected]) && earlier_process(&processes[i], &processes[selected]))) {
                selected = i;
            }
        }

        if (selected < 0) {
            int next_arrival = INT_MAX;
            for (int i = 0; i < count; ++i) {
                if (!done[i] && processes[i].arrival_time < next_arrival) {
                    next_arrival = processes[i].arrival_time;
                }
            }
            time = next_arrival;
            continue;
        }

        const int start = time;
        time += processes[selected].burst_time;
        add_segment(report, processes[selected].pid, start, time);
        set_result(report, selected, &processes[selected], start, time);
        done[selected] = true;
        ++completed;
    }

    compute_averages(report);
    return 0;
}

int schedule_sjf(const Process *processes, int count, ScheduleReport *report) {
    return schedule_non_preemptive(processes, count, report, score_burst);
}

int schedule_priority(const Process *processes, int count, ScheduleReport *report) {
    return schedule_non_preemptive(processes, count, report, score_priority);
}

static void sort_arrival_order(const Process *processes, int count, int *order) {
    for (int i = 0; i < count; ++i) {
        order[i] = i;
    }
    for (int i = 0; i < count - 1; ++i) {
        for (int j = i + 1; j < count; ++j) {
            if (earlier_process(&processes[order[j]], &processes[order[i]])) {
                const int tmp = order[i];
                order[i] = order[j];
                order[j] = tmp;
            }
        }
    }
}

int schedule_rr(const Process *processes, int count, int quantum, ScheduleReport *report) {
    if (quantum <= 0 || validate_input(processes, count, report) != 0) {
        return -1;
    }

    int order[OSLAB_MAX_PROCESSES];
    int remaining[OSLAB_MAX_PROCESSES];
    int first_start[OSLAB_MAX_PROCESSES];
    /* 环形就绪队列: 同一时刻队列中最多容纳 count 个进程, 容量取 count+1 即可避免越界 */
    int queue[OSLAB_MAX_PROCESSES + 1];
    const int queue_cap = OSLAB_MAX_PROCESSES + 1;
    int head = 0;
    int tail = 0;
    int queued = 0;
    int next_arrival = 0;
    int completed = 0;
    int time;

    sort_arrival_order(processes, count, order);
    for (int i = 0; i < count; ++i) {
        remaining[i] = processes[i].burst_time;
        first_start[i] = -1;
    }
    time = processes[order[0]].arrival_time;

    while (completed < count) {
        while (next_arrival < count && processes[order[next_arrival]].arrival_time <= time) {
            queue[tail] = order[next_arrival++];
            tail = (tail + 1) % queue_cap;
            ++queued;
        }

        if (queued == 0) {
            time = processes[order[next_arrival]].arrival_time;
            continue;
        }

        const int current = queue[head];
        head = (head + 1) % queue_cap;
        --queued;
        if (first_start[current] < 0) {
            first_start[current] = time;
        }
        const int run_time = remaining[current] < quantum ? remaining[current] : quantum;
        add_segment(report, processes[current].pid, time, time + run_time);
        time += run_time;
        remaining[current] -= run_time;

        while (next_arrival < count && processes[order[next_arrival]].arrival_time <= time) {
            queue[tail] = order[next_arrival++];
            tail = (tail + 1) % queue_cap;
            ++queued;
        }

        if (remaining[current] > 0) {
            queue[tail] = current;
            tail = (tail + 1) % queue_cap;
            ++queued;
        } else {
            set_result(report, current, &processes[current], first_start[current], time);
            ++completed;
        }
    }

    compute_averages(report);
    return 0;
}

void schedule_print_report(const ScheduleReport *report) {
    if (report == NULL) {
        return;
    }

    puts("\n运行顺序:");
    for (int i = 0; i < report->segment_count; ++i) {
        printf("P%d[%d-%d]%s",
               report->segments[i].pid,
               report->segments[i].start_time,
               report->segments[i].end_time,
               i == report->segment_count - 1 ? "\n" : " -> ");
    }

    puts("\nPID\t完成\t周转\t等待\t响应");
    for (int i = 0; i < report->process_count; ++i) {
        printf("P%d\t%d\t%d\t%d\t%d\n",
               report->results[i].pid,
               report->results[i].completion_time,
               report->results[i].turnaround_time,
               report->results[i].waiting_time,
               report->results[i].response_time);
    }
    printf("平均周转时间: %.2f\n", report->average_turnaround_time);
    printf("平均等待时间: %.2f\n", report->average_waiting_time);
    printf("平均响应时间: %.2f\n", report->average_response_time);
}
