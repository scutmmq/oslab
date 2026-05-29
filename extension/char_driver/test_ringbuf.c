#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>

#define DEV "/dev/oslab_ringbuf"

int main(void)
{
    const char *msg = "hello rb"; /* 8 bytes, fits buf_size>=16 */
    char buf[64] = {0};

    int fd = open(DEV, O_RDWR);
    if (fd < 0) { perror("open"); return 1; }

    ssize_t w = write(fd, msg, strlen(msg));
    assert(w == (ssize_t)strlen(msg));

    ssize_t r = read(fd, buf, sizeof(buf));
    assert(r == (ssize_t)strlen(msg));
    assert(strcmp(buf, msg) == 0);
    printf("[basic] wrote %zd, read %zd: \"%s\" OK\n", w, r, buf);

    close(fd);

    /* 此刻缓冲区应已被上面读空; 非阻塞空读应返回 EAGAIN */
    int fd2 = open(DEV, O_RDWR | O_NONBLOCK);
    assert(fd2 >= 0);
    char tmp[8];
    ssize_t rr = read(fd2, tmp, sizeof(tmp));
    assert(rr == -1 && errno == EAGAIN);
    printf("[nonblock] empty read returned EAGAIN OK\n");
    close(fd2);

    printf("test_ringbuf: all checks passed\n");
    return 0;
}
