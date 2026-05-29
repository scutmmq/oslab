#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>

#define DEV "/dev/oslab_ringbuf"
#define N   50      /* 记录条数 */
#define REC 8       /* 每条 "%07d\n" = 7 数字 + 换行 = 8 字节 */

/* 写满 n 字节, 处理短写 */
static int write_all(int fd, const char *p, size_t n)
{
    size_t done = 0;
    while (done < n) {
        ssize_t w = write(fd, p + done, n - done);
        if (w < 0) { perror("write"); return -1; }
        done += (size_t)w;
    }
    return 0;
}

/* 读满 n 字节, 处理短读 */
static int read_all(int fd, char *p, size_t n)
{
    size_t done = 0;
    while (done < n) {
        ssize_t r = read(fd, p + done, n - done);
        if (r < 0) { perror("read"); return -1; }
        if (r == 0) { fprintf(stderr, "unexpected EOF\n"); return -1; }
        done += (size_t)r;
    }
    return 0;
}

int main(void)
{
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return 1; }

    if (pid == 0) {
        /* 子进程: 生产者 */
        int fd = open(DEV, O_WRONLY);
        if (fd < 0) { perror("open producer"); _exit(1); }
        for (int i = 0; i < N; i++) {
            char rec[REC + 1];
            snprintf(rec, sizeof(rec), "%07d\n", i);
            if (write_all(fd, rec, REC) < 0) { close(fd); _exit(1); }
        }
        close(fd);
        _exit(0);
    }

    /* 父进程: 消费者 */
    int fd = open(DEV, O_RDONLY);
    if (fd < 0) { perror("open consumer"); return 1; }

    int seen[N];
    memset(seen, 0, sizeof(seen));
    int dup = 0, bad = 0;
    for (int i = 0; i < N; i++) {
        char rec[REC + 1] = {0};
        if (read_all(fd, rec, REC) < 0) { close(fd); return 1; }
        int v = atoi(rec);
        if (v < 0 || v >= N) { bad++; continue; }
        if (seen[v]++) dup++;
    }
    close(fd);

    int status = 0;
    waitpid(pid, &status, 0);

    int missing = 0;
    for (int i = 0; i < N; i++)
        if (seen[i] != 1) missing++;

    printf("pc_demo: produced=%d consumed=%d dup=%d bad=%d missing=%d\n",
           N, N, dup, bad, missing);
    if (dup == 0 && bad == 0 && missing == 0) {
        printf("pc_demo: PASS (each item consumed exactly once)\n");
        return 0;
    }
    printf("pc_demo: FAIL\n");
    return 1;
}
