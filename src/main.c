#include "oslab/filesystem.h"
#include "oslab/memory.h"
#include "oslab/scheduler.h"
#include "oslab/sync_demo.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

static void clear_input(void) {
    int ch;
    while ((ch = getchar()) != '\n' && ch != EOF) {
    }
}

static int read_int(const char *prompt, int *value) {
    printf("%s", prompt);
    if (scanf("%d", value) != 1) {
        clear_input();
        return -1;
    }
    return 0;
}

static void read_text(const char *prompt, char *buffer, int size) {
    printf("%s", prompt);
    clear_input();
    if (fgets(buffer, size, stdin) == NULL) {
        buffer[0] = '\0';
        return;
    }
    buffer[strcspn(buffer, "\n")] = '\0';
}

static void compare_algorithms(const Process *processes, int count) {
    int quantum;
    if (read_int("时间片大小(用于 RR): ", &quantum) != 0 || quantum <= 0) {
        puts("时间片无效。");
        return;
    }

    ScheduleReport report;
    puts("\n算法\t\t平均周转\t平均等待\t平均响应");
    if (schedule_fcfs(processes, count, &report) == 0) {
        printf("FCFS\t\t%.2f\t\t%.2f\t\t%.2f\n",
               report.average_turnaround_time, report.average_waiting_time, report.average_response_time);
    }
    if (schedule_sjf(processes, count, &report) == 0) {
        printf("SJF\t\t%.2f\t\t%.2f\t\t%.2f\n",
               report.average_turnaround_time, report.average_waiting_time, report.average_response_time);
    }
    if (schedule_rr(processes, count, quantum, &report) == 0) {
        printf("RR(q=%d)\t\t%.2f\t\t%.2f\t\t%.2f\n",
               quantum, report.average_turnaround_time, report.average_waiting_time, report.average_response_time);
    }
    if (schedule_priority(processes, count, &report) == 0) {
        printf("优先级\t\t%.2f\t\t%.2f\t\t%.2f\n",
               report.average_turnaround_time, report.average_waiting_time, report.average_response_time);
    }
    puts("(平均周转/等待/响应时间越小, 该算法在本组进程上表现越好)");
}

static void scheduling_menu(void) {
    Process processes[OSLAB_MAX_PROCESSES];
    ScheduleReport report;
    int count;
    int algorithm;
    int quantum = 1;

    puts("\n--- 处理机调度 ---");
    puts("说明: 先输入进程数量, 再依次输入每个进程的 pid 到达时间 服务时间 优先级");
    puts("      优先级数值越小优先级越高; 选择 RR 时会再询问时间片大小");
    printf("      进程数量范围 1~%d, 四个数字用空格隔开\n", OSLAB_MAX_PROCESSES);
    puts("示例: 数量输入 3, 第一个进程输入 \"1 0 4 2\" (pid=1 到达=0 服务=4 优先级=2)");

    if (read_int("进程数量: ", &count) != 0 || count <= 0 || count > OSLAB_MAX_PROCESSES) {
        puts("进程数量无效。");
        return;
    }

    for (int i = 0; i < count; ++i) {
        printf("进程 %d 的 pid 到达时间 服务时间 优先级: ", i + 1);
        if (scanf("%d %d %d %d",
                  &processes[i].pid,
                  &processes[i].arrival_time,
                  &processes[i].burst_time,
                  &processes[i].priority) != 4) {
            clear_input();
            puts("输入无效。");
            return;
        }
    }

    puts("\n1. FCFS  2. SJF  3. RR  4. 优先级(数值越小越高)  5. 四种算法对比");
    if (read_int("选择算法: ", &algorithm) != 0) {
        return;
    }

    if (algorithm == 5) {
        compare_algorithms(processes, count);
        return;
    }

    int rc = -1;
    if (algorithm == 1) {
        rc = schedule_fcfs(processes, count, &report);
    } else if (algorithm == 2) {
        rc = schedule_sjf(processes, count, &report);
    } else if (algorithm == 3) {
        if (read_int("时间片大小: ", &quantum) != 0) {
            return;
        }
        rc = schedule_rr(processes, count, quantum, &report);
    } else if (algorithm == 4) {
        rc = schedule_priority(processes, count, &report);
    }

    if (rc != 0) {
        puts("调度失败，请检查进程参数。");
        return;
    }
    schedule_print_report(&report);
    puts("(指标含义: 周转=完成-到达, 等待=周转-服务, 响应=首次运行-到达)");
}

static void page_replacement_menu(void) {
    int refs[OSLAB_MAX_REFERENCES];
    int count;
    int frames;
    int algorithm;
    PageReport report;

    puts("\n--- 页面置换 ---");
    printf("说明: 先输入页框数量(1~%d)和访问序列长度(1~%d), 再输入页号序列\n",
           OSLAB_MAX_FRAMES, OSLAB_MAX_REFERENCES);
    puts("      页号用空格隔开; 结果会显示每一步的内存块状态与缺页/命中");
    puts("示例: 页框=3, 长度=7, 序列 \"1 2 3 2 4 1 2\"");

    if (read_int("页框数量: ", &frames) != 0 ||
        read_int("访问序列长度: ", &count) != 0 ||
        count <= 0 || count > OSLAB_MAX_REFERENCES) {
        puts("输入无效。");
        return;
    }
    printf("输入访问序列(%d 个页号): ", count);
    for (int i = 0; i < count; ++i) {
        if (scanf("%d", &refs[i]) != 1) {
            clear_input();
            puts("输入无效。");
            return;
        }
    }
    puts("\n1. FIFO  2. LRU");
    if (read_int("选择算法: ", &algorithm) != 0) {
        return;
    }

    const int rc = algorithm == 1 ? page_fifo(refs, count, frames, &report)
                                  : page_lru(refs, count, frames, &report);
    if (rc != 0) {
        puts("页面置换模拟失败。");
        return;
    }
    page_print_report(&report);
}

static void partition_menu(void) {
    PartitionTable table;
    int total;
    puts("\n--- 动态分区分配 ---");
    puts("说明: 先输入内存总大小, 然后循环进行分配/回收/显示");
    puts("      分配需输入 pid(正整数) 和大小; 回收输入已分配的 pid");
    puts("      相邻空闲分区会自动合并; 选 0 返回上级菜单");
    if (read_int("内存总大小: ", &total) != 0 || total <= 0) {
        puts("内存大小无效。");
        return;
    }
    partition_init(&table, total);

    for (;;) {
        int choice;
        int pid;
        int size;
        puts("\n1. 首次适应分配  2. 最佳适应分配  3. 回收  4. 显示分区  0. 返回");
        if (read_int("选择: ", &choice) != 0) {
            return;
        }
        if (choice == 0) {
            return;
        } else if (choice == 1 || choice == 2) {
            if (read_int("pid: ", &pid) != 0 || read_int("大小: ", &size) != 0) {
                return;
            }
            const int start = choice == 1 ? partition_allocate_first_fit(&table, pid, size)
                                          : partition_allocate_best_fit(&table, pid, size);
            if (start < 0) {
                puts("分配失败。");
            } else {
                printf("分配成功，起始地址: %d\n", start);
            }
        } else if (choice == 3) {
            if (read_int("pid: ", &pid) != 0) {
                return;
            }
            puts(partition_free(&table, pid) == 0 ? "回收成功。" : "回收失败。");
        } else if (choice == 4) {
            partition_print(&table);
        }
    }
}

static void memory_menu(void) {
    int choice;
    puts("\n1. 页面置换  2. 动态分区");
    if (read_int("选择: ", &choice) != 0) {
        return;
    }
    if (choice == 1) {
        page_replacement_menu();
    } else if (choice == 2) {
        partition_menu();
    }
}

static void sync_menu(void) {
    int choice;
    puts("\n--- 进程同步与并发控制 ---");
    puts("说明: 选择后会创建多线程运行演示, 输出为各线程交替打印");
    puts("      因并发调度, 每次运行的打印顺序可能不同, 属正常现象");
    puts("\n1. 生产者-消费者  2. 读者-写者  3. 哲学家进餐");
    if (read_int("选择: ", &choice) != 0) {
        return;
    }
    if (choice == 1) {
        run_producer_consumer_demo();
    } else if (choice == 2) {
        run_readers_writers_demo();
    } else if (choice == 3) {
        run_dining_philosophers_demo();
    }
}

static void filesystem_menu(void) {
    SimpleFS fs;
    char name[FS_MAX_FILENAME];
    char content[FS_BLOCK_SIZE * FS_MAX_FILE_BLOCKS + 1];
    fs_format(&fs);

    puts("\n--- 文件系统 ---");
    printf("说明: 块大小 %d 字节, 共 %d 块, 单个文件最多 %d 块\n",
           FS_BLOCK_SIZE, FS_BLOCK_COUNT, FS_MAX_FILE_BLOCKS);
    puts("      请先创建文件再写入/读取; 再次写入会覆盖原内容; 文件系统仅存于内存, 退出即清空");

    for (;;) {
        int choice;
        puts("\n1. 创建  2. 写入  3. 读取  4. 删除  5. 列目录  6. 格式化  0. 返回");
        if (read_int("选择: ", &choice) != 0) {
            return;
        }
        if (choice == 0) {
            return;
        } else if (choice == 1) {
            read_text("文件名: ", name, sizeof(name));
            puts(fs_create(&fs, name) == 0 ? "创建成功。" : "创建失败。");
        } else if (choice == 2) {
            read_text("文件名: ", name, sizeof(name));
            read_text("内容: ", content, sizeof(content));
            puts(fs_write(&fs, name, content) == 0 ? "写入成功。" : "写入失败。");
        } else if (choice == 3) {
            read_text("文件名: ", name, sizeof(name));
            const int bytes = fs_read(&fs, name, content, sizeof(content));
            if (bytes < 0) {
                puts("读取失败。");
            } else {
                printf("读取 %d 字节: %s\n", bytes, content);
            }
        } else if (choice == 4) {
            read_text("文件名: ", name, sizeof(name));
            puts(fs_delete(&fs, name) == 0 ? "删除成功。" : "删除失败。");
        } else if (choice == 5) {
            fs_list(&fs);
        } else if (choice == 6) {
            fs_format(&fs);
            puts("格式化完成。");
        }
    }
}

int main(void) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    for (;;) {
        int choice;
        puts("\n==== OS 课程设计 3.1 基础必做 ====");
        puts("(输入对应数字选择功能, 进入模块后按提示输入参数)");
        puts("1. 处理机调度");
        puts("2. 内存管理");
        puts("3. 进程同步与并发控制");
        puts("4. 文件系统");
        puts("0. 退出");
        if (read_int("选择模块: ", &choice) != 0) {
            puts("输入无效。");
            continue;
        }
        if (choice == 0) {
            break;
        } else if (choice == 1) {
            scheduling_menu();
        } else if (choice == 2) {
            memory_menu();
        } else if (choice == 3) {
            sync_menu();
        } else if (choice == 4) {
            filesystem_menu();
        }
    }
    return 0;
}
