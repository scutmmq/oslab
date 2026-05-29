# Linux 内核态环形缓冲区字符设备驱动 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 实现一个 Linux 字符设备驱动 `/dev/oslab_ringbuf`，内核态维护环形缓冲区，用户进程经 read/write 充当生产者-消费者，用内核 mutex + 等待队列实现阻塞同步，并经 `/proc/oslab_ringbuf` 导出统计。

**Architecture:** 单个内核模块（`oslab_ringbuf.c`）注册字符设备与 `/proc` 项；内核环形缓冲区由一把 mutex 与两个等待队列保护，提供阻塞 / `O_NONBLOCK` 读写；两个用户态程序（断言测试 + fork 并发演示）验证行为；kbuild Makefile 与脚本完成构建 / 加载 / 测试。

**Tech Stack:** C（内核模块 API：cdev / class / proc_ops / mutex / wait_queue）、kbuild、gcc 用户态程序、Linux 5.6+。

---

## 执行前提（务必先读）

1. **环境**：本扩展只能在 **Linux** 上构建运行（你有虚拟机 / 双系统 / 远程）。把项目复制到 Linux 主机，或用共享目录 / WSL2。所有命令均在 Linux shell 执行。
2. **依赖**：需安装内核头文件与构建工具：
   - Ubuntu/Debian：`sudo apt install build-essential linux-headers-$(uname -r)`
3. **权限**：加载 / 卸载模块需 `sudo`。
4. **git**：本计划含提交步骤。若项目根目录尚未初始化 git，先在 Linux 端项目根执行 `git init`（课程也要求 GitHub 托管，方向一致）。
5. **内核版本**：本代码目标 5.6+；`class_create` 6.4+ 用单参形式，代码已用 `LINUX_VERSION_CODE` 宏自动适配。先记录 `uname -r` 备查。

## File Structure

全部新增于 `extension/char_driver/`，不触碰现有 CMake 工程：

| 文件 | 职责 |
|------|------|
| `extension/char_driver/oslab_ringbuf.c` | 内核模块：字符设备 + 环形缓冲 + 同步 + /proc |
| `extension/char_driver/Makefile` | kbuild 模块构建 + 用户程序 + load/unload/test 目标 |
| `extension/char_driver/test_ringbuf.c` | 用户态正确性测试（写读断言 + O_NONBLOCK EAGAIN） |
| `extension/char_driver/pc_demo.c` | 用户态并发演示（fork 生产者+消费者，不丢不重） |
| `extension/char_driver/load.sh` | insmod + chmod 设备节点 |
| `extension/char_driver/unload.sh` | rmmod |
| `extension/char_driver/README.md` | 构建 / 加载 / 测试步骤 + 报告素材 |

---

## Task 1: 可加载的模块骨架 + 构建系统

建立最小可加载内核模块与 kbuild Makefile，验证加载 / 卸载闭环。

**Files:**
- Create: `extension/char_driver/oslab_ringbuf.c`
- Create: `extension/char_driver/Makefile`

- [ ] **Step 1: （若需）初始化 git**

Run（在项目根目录）：
```bash
git rev-parse --is-inside-work-tree 2>/dev/null || git init
```
Expected: 已是 git 仓库则输出 `true`；否则初始化新仓库。

- [ ] **Step 2: 记录内核版本与确认头文件**

Run:
```bash
uname -r
ls /lib/modules/$(uname -r)/build >/dev/null && echo "headers OK"
```
Expected: 打印内核版本，并输出 `headers OK`。若失败，先按"执行前提 2"安装头文件。

- [ ] **Step 3: 写最小内核模块骨架**

Create `extension/char_driver/oslab_ringbuf.c`:
```c
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

static int __init rb_init(void)
{
    pr_info("oslab_ringbuf: module loaded\n");
    return 0;
}

static void __exit rb_exit(void)
{
    pr_info("oslab_ringbuf: module unloaded\n");
}

module_init(rb_init);
module_exit(rb_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("OS course design");
MODULE_DESCRIPTION("Producer-consumer ring buffer character device");
```

- [ ] **Step 4: 写 Makefile**

Create `extension/char_driver/Makefile`:
```make
obj-m += oslab_ringbuf.o

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)
CC   ?= gcc
CFLAGS_USER := -Wall -Wextra -O2

.PHONY: all module user clean load unload test

all: module user

module:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

user: test_ringbuf pc_demo

test_ringbuf: test_ringbuf.c
	$(CC) $(CFLAGS_USER) -o $@ $<

pc_demo: pc_demo.c
	$(CC) $(CFLAGS_USER) -o $@ $<

load: module
	sudo insmod oslab_ringbuf.ko
	sudo chmod 666 /dev/oslab_ringbuf

unload:
	sudo rmmod oslab_ringbuf

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f test_ringbuf pc_demo

test: all
	-sudo rmmod oslab_ringbuf 2>/dev/null || true
	sudo insmod oslab_ringbuf.ko buf_size=16
	sudo chmod 666 /dev/oslab_ringbuf
	@echo "=== test_ringbuf ===" ; ./test_ringbuf
	@echo "=== pc_demo ===" ; ./pc_demo
	@echo "=== /proc/oslab_ringbuf ===" ; cat /proc/oslab_ringbuf
	sudo rmmod oslab_ringbuf
	@echo "=== ALL TESTS DONE ==="
```

注：`user`/`test_ringbuf`/`pc_demo` 目标此刻还无对应源文件，本任务只构建并测试 `module`。

- [ ] **Step 5: 构建模块**

Run:
```bash
cd extension/char_driver && make module
```
Expected: 生成 `oslab_ringbuf.ko`，无报错（可能有许可/版本提示，可忽略）。

- [ ] **Step 6: 加载并验证（应在 dmesg 看到 loaded）**

Run:
```bash
sudo insmod oslab_ringbuf.ko && lsmod | grep oslab_ringbuf && sudo dmesg | tail -n 3
```
Expected: `lsmod` 列出 `oslab_ringbuf`；dmesg 末尾出现 `oslab_ringbuf: module loaded`。

- [ ] **Step 7: 卸载并验证**

Run:
```bash
sudo rmmod oslab_ringbuf && sudo dmesg | tail -n 2
```
Expected: dmesg 出现 `oslab_ringbuf: module unloaded`；`lsmod | grep oslab_ringbuf` 无输出。

- [ ] **Step 8: 提交**

```bash
git add extension/char_driver/oslab_ringbuf.c extension/char_driver/Makefile
git commit -m "feat(driver): loadable kernel module skeleton + kbuild Makefile"
```

---

## Task 2: 字符设备 + 环形缓冲 + 非阻塞读写

注册 `/dev/oslab_ringbuf`，加入环形缓冲区与 `copy_to/from_user` 读写（暂不阻塞：空读返回 0、满写短写），并用单进程断言测试验证写读一致。

**Files:**
- Modify: `extension/char_driver/oslab_ringbuf.c`（整体替换为下方 v1）
- Create: `extension/char_driver/test_ringbuf.c`

- [ ] **Step 1: 写正确性测试（先失败）**

Create `extension/char_driver/test_ringbuf.c`:
```c
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
    printf("test_ringbuf: all checks passed\n");
    return 0;
}
```

- [ ] **Step 2: 编译测试程序，确认现在无法通过（设备不存在）**

Run:
```bash
make test_ringbuf && ./test_ringbuf
```
Expected: 编译成功；运行时 `open` 失败打印 `open: No such file or directory` 并返回 1（因为还没实现字符设备）。

- [ ] **Step 3: 整体替换 oslab_ringbuf.c 为带字符设备 + 缓冲的 v1**

Replace entire `extension/char_driver/oslab_ringbuf.c`:
```c
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/minmax.h>
#include <linux/version.h>

#define DEVICE_NAME "oslab_ringbuf"

static int buf_size = 1024;
module_param(buf_size, int, 0444);
MODULE_PARM_DESC(buf_size, "Ring buffer size in bytes (default 1024)");

/* device infrastructure */
static dev_t dev_num;
static struct cdev rb_cdev;
static struct class *rb_class;

/* ring buffer state (protected by rb_lock) */
static char *buffer;
static size_t head;   /* write index */
static size_t tail;   /* read index */
static size_t count;  /* bytes currently stored */
static DEFINE_MUTEX(rb_lock);

static int rb_open(struct inode *inode, struct file *filp)
{
    pr_info("oslab_ringbuf: opened (pid=%d)\n", current->pid);
    return 0;
}

static int rb_release(struct inode *inode, struct file *filp)
{
    pr_info("oslab_ringbuf: released (pid=%d)\n", current->pid);
    return 0;
}

static ssize_t rb_read(struct file *filp, char __user *ubuf, size_t len, loff_t *off)
{
    ssize_t ret;
    size_t to_copy, first;

    if (len == 0)
        return 0;
    if (mutex_lock_interruptible(&rb_lock))
        return -ERESTARTSYS;

    if (count == 0) {            /* v1: 非阻塞, 空则返回 0 */
        ret = 0;
        goto out;
    }

    to_copy = min(len, count);
    first = min(to_copy, (size_t)buf_size - tail);
    if (copy_to_user(ubuf, buffer + tail, first)) {
        ret = -EFAULT;
        goto out;
    }
    if (to_copy > first &&
        copy_to_user(ubuf + first, buffer, to_copy - first)) {
        ret = -EFAULT;
        goto out;
    }
    tail = (tail + to_copy) % buf_size;
    count -= to_copy;
    ret = to_copy;
out:
    mutex_unlock(&rb_lock);
    return ret;
}

static ssize_t rb_write(struct file *filp, const char __user *ubuf, size_t len, loff_t *off)
{
    ssize_t ret;
    size_t space, to_copy, first;

    if (len == 0)
        return 0;
    if (mutex_lock_interruptible(&rb_lock))
        return -ERESTARTSYS;

    if (count == (size_t)buf_size) {  /* v1: 非阻塞, 满则返回 0 */
        ret = 0;
        goto out;
    }

    space = (size_t)buf_size - count;
    to_copy = min(len, space);
    first = min(to_copy, (size_t)buf_size - head);
    if (copy_from_user(buffer + head, ubuf, first)) {
        ret = -EFAULT;
        goto out;
    }
    if (to_copy > first &&
        copy_from_user(buffer, ubuf + first, to_copy - first)) {
        ret = -EFAULT;
        goto out;
    }
    head = (head + to_copy) % buf_size;
    count += to_copy;
    ret = to_copy;
out:
    mutex_unlock(&rb_lock);
    return ret;
}

static const struct file_operations rb_fops = {
    .owner = THIS_MODULE,
    .open = rb_open,
    .release = rb_release,
    .read = rb_read,
    .write = rb_write,
};

static int __init rb_init(void)
{
    int ret;
    struct device *dev;

    if (buf_size <= 0) {
        pr_err("oslab_ringbuf: invalid buf_size %d\n", buf_size);
        return -EINVAL;
    }

    buffer = kmalloc(buf_size, GFP_KERNEL);
    if (!buffer)
        return -ENOMEM;
    head = tail = count = 0;

    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0)
        goto err_free;

    cdev_init(&rb_cdev, &rb_fops);
    rb_cdev.owner = THIS_MODULE;
    ret = cdev_add(&rb_cdev, dev_num, 1);
    if (ret < 0)
        goto err_region;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
    rb_class = class_create(DEVICE_NAME);
#else
    rb_class = class_create(THIS_MODULE, DEVICE_NAME);
#endif
    if (IS_ERR(rb_class)) {
        ret = PTR_ERR(rb_class);
        goto err_cdev;
    }

    dev = device_create(rb_class, NULL, dev_num, NULL, DEVICE_NAME);
    if (IS_ERR(dev)) {
        ret = PTR_ERR(dev);
        goto err_class;
    }

    pr_info("oslab_ringbuf: loaded, major=%d buf_size=%d\n",
            MAJOR(dev_num), buf_size);
    return 0;

err_class:
    class_destroy(rb_class);
err_cdev:
    cdev_del(&rb_cdev);
err_region:
    unregister_chrdev_region(dev_num, 1);
err_free:
    kfree(buffer);
    return ret;
}

static void __exit rb_exit(void)
{
    device_destroy(rb_class, dev_num);
    class_destroy(rb_class);
    cdev_del(&rb_cdev);
    unregister_chrdev_region(dev_num, 1);
    kfree(buffer);
    pr_info("oslab_ringbuf: unloaded\n");
}

module_init(rb_init);
module_exit(rb_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("OS course design");
MODULE_DESCRIPTION("Producer-consumer ring buffer character device");
```

- [ ] **Step 4: 重新构建并加载**

Run:
```bash
make module && sudo rmmod oslab_ringbuf 2>/dev/null; sudo insmod oslab_ringbuf.ko && sudo chmod 666 /dev/oslab_ringbuf && ls -l /dev/oslab_ringbuf
```
Expected: 构建无错；`/dev/oslab_ringbuf` 存在且权限 `rw-rw-rw-`。

- [ ] **Step 5: 运行正确性测试（应通过）**

Run:
```bash
./test_ringbuf
```
Expected:
```
[basic] wrote 8, read 8: "hello rb" OK
test_ringbuf: all checks passed
```

- [ ] **Step 6: 卸载，验证设备节点消失**

Run:
```bash
sudo rmmod oslab_ringbuf && ls /dev/oslab_ringbuf 2>&1
```
Expected: 输出 `ls: cannot access '/dev/oslab_ringbuf': No such file or directory`。

- [ ] **Step 7: 提交**

```bash
git add extension/char_driver/oslab_ringbuf.c extension/char_driver/test_ringbuf.c
git commit -m "feat(driver): char device + ring buffer with non-blocking read/write"
```

---

## Task 3: 统计计数 + /proc 读接口

加入统计计数与 `/proc/oslab_ringbuf` 只读统计输出（基于 seq_file）。本任务声明全部计数字段，但阻塞计数（read/write_block）留待 Task 4 自增。

**Files:**
- Modify: `extension/char_driver/oslab_ringbuf.c`（按下述精确位置增改）

- [ ] **Step 1: 添加头文件与统计字段**

在 `oslab_ringbuf.c` 顶部 include 区，`#include <linux/version.h>` 之后追加：
```c
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
```

在 `static DEFINE_MUTEX(rb_lock);` 之后追加统计字段：
```c
/* statistics (protected by rb_lock) */
static unsigned long total_written;
static unsigned long total_read;
static unsigned long open_count;
static unsigned long read_block_count;
static unsigned long write_block_count;
```

- [ ] **Step 2: 在 rb_open 中累加 open_count**

把 `rb_open` 函数体替换为：
```c
static int rb_open(struct inode *inode, struct file *filp)
{
    mutex_lock(&rb_lock);
    open_count++;
    mutex_unlock(&rb_lock);
    pr_info("oslab_ringbuf: opened (pid=%d)\n", current->pid);
    return 0;
}
```

- [ ] **Step 3: 在 rb_read 成功路径累加 total_read**

在 `rb_read` 中，`count -= to_copy;` 之后、`ret = to_copy;` 之前插入一行：
```c
    total_read += to_copy;
```

- [ ] **Step 4: 在 rb_write 成功路径累加 total_written**

在 `rb_write` 中，`count += to_copy;` 之后、`ret = to_copy;` 之前插入一行：
```c
    total_written += to_copy;
```

- [ ] **Step 5: 添加 /proc 实现（放在 rb_fops 定义之后）**

在 `static const struct file_operations rb_fops = {...};` 之后追加：
```c
#define PROC_NAME "oslab_ringbuf"

static int rb_proc_show(struct seq_file *m, void *v)
{
    mutex_lock(&rb_lock);
    seq_printf(m, "buffer_size:       %d\n", buf_size);
    seq_printf(m, "current_usage:     %zu\n", count);
    seq_printf(m, "total_written:     %lu\n", total_written);
    seq_printf(m, "total_read:        %lu\n", total_read);
    seq_printf(m, "open_count:        %lu\n", open_count);
    seq_printf(m, "read_block_count:  %lu\n", read_block_count);
    seq_printf(m, "write_block_count: %lu\n", write_block_count);
    mutex_unlock(&rb_lock);
    return 0;
}

static int rb_proc_open(struct inode *inode, struct file *filp)
{
    return single_open(filp, rb_proc_show, NULL);
}

static const struct proc_ops rb_proc_ops = {
    .proc_open = rb_proc_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};
```

- [ ] **Step 6: 在 init 中创建 /proc，在 exit 中移除**

在 `rb_init` 内，`dev = device_create(...)` 的错误检查块之后、`pr_info("oslab_ringbuf: loaded...")` 之前插入：
```c
    if (!proc_create(PROC_NAME, 0666, NULL, &rb_proc_ops)) {
        ret = -ENOMEM;
        goto err_device;
    }
```

把 `rb_init` 末尾的错误处理标签段（从 `err_class:` 起）替换为（新增 `err_device:` 与 `err_class:` 之间顺序）：
```c
err_device:
    device_destroy(rb_class, dev_num);
err_class:
    class_destroy(rb_class);
err_cdev:
    cdev_del(&rb_cdev);
err_region:
    unregister_chrdev_region(dev_num, 1);
err_free:
    kfree(buffer);
    return ret;
```

在 `rb_exit` 函数体最前面（`device_destroy` 之前）插入：
```c
    remove_proc_entry(PROC_NAME, NULL);
```

- [ ] **Step 7: 构建、加载、验证 /proc 输出**

Run:
```bash
make module && sudo rmmod oslab_ringbuf 2>/dev/null; sudo insmod oslab_ringbuf.ko && sudo chmod 666 /dev/oslab_ringbuf
./test_ringbuf
cat /proc/oslab_ringbuf
```
Expected: `test_ringbuf` 通过；`/proc/oslab_ringbuf` 输出 7 行统计，其中 `total_written: 8`、`total_read: 8`、`open_count` ≥ 1、`current_usage: 0`、阻塞计数为 0。

- [ ] **Step 8: 卸载并验证 /proc 移除**

Run:
```bash
sudo rmmod oslab_ringbuf && cat /proc/oslab_ringbuf 2>&1
```
Expected: 输出 `cat: /proc/oslab_ringbuf: No such file or directory`。

- [ ] **Step 9: 提交**

```bash
git add extension/char_driver/oslab_ringbuf.c
git commit -m "feat(driver): statistics counters and /proc read interface"
```

---

## Task 4: 阻塞读写（等待队列）+ O_NONBLOCK + /proc reset

加入两个等待队列，实现空读 / 满写阻塞、`O_NONBLOCK` 返回 `-EAGAIN`、信号中断 `-ERESTARTSYS`，自增阻塞计数，并支持向 `/proc` 写 `reset` 清零统计。

**Files:**
- Modify: `extension/char_driver/oslab_ringbuf.c`（按精确位置增改）
- Modify: `extension/char_driver/test_ringbuf.c`（追加 O_NONBLOCK 用例）

- [ ] **Step 1: 在 test_ringbuf.c 追加 O_NONBLOCK EAGAIN 用例**

在 `test_ringbuf.c` 的 `close(fd);` 之后、`printf("test_ringbuf: all checks passed\n");` 之前插入：
```c
    /* 此刻缓冲区应已被上面读空; 非阻塞空读应返回 EAGAIN */
    int fd2 = open(DEV, O_RDWR | O_NONBLOCK);
    assert(fd2 >= 0);
    char tmp[8];
    ssize_t rr = read(fd2, tmp, sizeof(tmp));
    assert(rr == -1 && errno == EAGAIN);
    printf("[nonblock] empty read returned EAGAIN OK\n");
    close(fd2);
```

- [ ] **Step 2: 重新编译测试，确认当前实现下新用例失败**

Run:
```bash
make test_ringbuf && (sudo rmmod oslab_ringbuf 2>/dev/null; sudo insmod oslab_ringbuf.ko && sudo chmod 666 /dev/oslab_ringbuf); ./test_ringbuf; echo "exit=$?"
```
Expected: 程序在 `[nonblock]` 断言处中止（assertion failed），`exit` 非 0。原因：v1 空读返回 0 而非 EAGAIN。

- [ ] **Step 3: 添加等待队列声明**

在 `oslab_ringbuf.c` 顶部，把 include 区追加：
```c
#include <linux/wait.h>
```
在 `static DEFINE_MUTEX(rb_lock);` 之后追加：
```c
static DECLARE_WAIT_QUEUE_HEAD(read_wq);
static DECLARE_WAIT_QUEUE_HEAD(write_wq);
```

- [ ] **Step 4: 替换 rb_read 的"空"分支为阻塞逻辑**

在 `rb_read` 中，把：
```c
    if (count == 0) {            /* v1: 非阻塞, 空则返回 0 */
        ret = 0;
        goto out;
    }
```
替换为：
```c
    while (count == 0) {
        if (filp->f_flags & O_NONBLOCK) {
            mutex_unlock(&rb_lock);
            return -EAGAIN;
        }
        read_block_count++;
        mutex_unlock(&rb_lock);
        if (wait_event_interruptible(read_wq, count > 0))
            return -ERESTARTSYS;
        if (mutex_lock_interruptible(&rb_lock))
            return -ERESTARTSYS;
    }
```
并在 `rb_read` 成功路径 `ret = to_copy;` 之后插入唤醒写者：
```c
    wake_up_interruptible(&write_wq);
```

- [ ] **Step 5: 替换 rb_write 的"满"分支为阻塞逻辑**

在 `rb_write` 中，把：
```c
    if (count == (size_t)buf_size) {  /* v1: 非阻塞, 满则返回 0 */
        ret = 0;
        goto out;
    }
```
替换为：
```c
    while (count == (size_t)buf_size) {
        if (filp->f_flags & O_NONBLOCK) {
            mutex_unlock(&rb_lock);
            return -EAGAIN;
        }
        write_block_count++;
        mutex_unlock(&rb_lock);
        if (wait_event_interruptible(write_wq, count < (size_t)buf_size))
            return -ERESTARTSYS;
        if (mutex_lock_interruptible(&rb_lock))
            return -ERESTARTSYS;
    }
```
并在 `rb_write` 成功路径 `ret = to_copy;` 之后插入唤醒读者：
```c
    wake_up_interruptible(&read_wq);
```

- [ ] **Step 6: 添加 /proc 写 reset**

在 `rb_proc_open` 之后、`rb_proc_ops` 定义之前追加：
```c
static ssize_t rb_proc_write(struct file *filp, const char __user *ubuf,
                             size_t len, loff_t *off)
{
    char kbuf[16];
    size_t n = min(len, sizeof(kbuf) - 1);

    if (copy_from_user(kbuf, ubuf, n))
        return -EFAULT;
    kbuf[n] = '\0';
    if (strncmp(kbuf, "reset", 5) == 0) {
        mutex_lock(&rb_lock);
        total_written = 0;
        total_read = 0;
        open_count = 0;
        read_block_count = 0;
        write_block_count = 0;
        mutex_unlock(&rb_lock);
        pr_info("oslab_ringbuf: stats reset\n");
    }
    return len;
}
```
把 `rb_proc_ops` 增加 `.proc_write`：
```c
static const struct proc_ops rb_proc_ops = {
    .proc_open = rb_proc_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
    .proc_write = rb_proc_write,
};
```

- [ ] **Step 7: 构建、加载、跑测试（含 O_NONBLOCK 用例）**

Run:
```bash
make module && (sudo rmmod oslab_ringbuf 2>/dev/null; sudo insmod oslab_ringbuf.ko && sudo chmod 666 /dev/oslab_ringbuf); ./test_ringbuf; echo "exit=$?"
```
Expected:
```
[basic] wrote 8, read 8: "hello rb" OK
[nonblock] empty read returned EAGAIN OK
test_ringbuf: all checks passed
exit=0
```

- [ ] **Step 8: 验证 /proc reset**

Run:
```bash
cat /proc/oslab_ringbuf | grep total_written
echo reset | sudo tee /proc/oslab_ringbuf >/dev/null
cat /proc/oslab_ringbuf | grep total_written
```
Expected: 第一次 `total_written` 为非零（如 8），reset 后变为 `total_written: 0`。

- [ ] **Step 9: 卸载并提交**

```bash
sudo rmmod oslab_ringbuf
git add extension/char_driver/oslab_ringbuf.c extension/char_driver/test_ringbuf.c
git commit -m "feat(driver): blocking I/O via wait queues, O_NONBLOCK, /proc reset"
```

---

## Task 5: 并发演示程序 pc_demo

用 `fork` 起生产者 + 消费者两个进程，配合小缓冲（`buf_size=16`）逼出阻塞，验证生产的每条记录被消费**恰好一次、不丢不重**。

**Files:**
- Create: `extension/char_driver/pc_demo.c`

- [ ] **Step 1: 写并发演示程序**

Create `extension/char_driver/pc_demo.c`:
```c
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
```

- [ ] **Step 2: 编译**

Run:
```bash
make pc_demo
```
Expected: 生成 `pc_demo` 可执行文件，无警告。

- [ ] **Step 3: 用小缓冲加载模块以逼出阻塞**

Run:
```bash
sudo rmmod oslab_ringbuf 2>/dev/null; sudo insmod oslab_ringbuf.ko buf_size=16 && sudo chmod 666 /dev/oslab_ringbuf
```
Expected: 加载成功；`dmesg | tail -1` 显示 `buf_size=16`。

- [ ] **Step 4: 运行并发演示**

Run:
```bash
./pc_demo; echo "exit=$?"
```
Expected:
```
pc_demo: produced=50 consumed=50 dup=0 bad=0 missing=0
pc_demo: PASS (each item consumed exactly once)
exit=0
```

- [ ] **Step 5: 查看阻塞确实发生（统计应非零）**

Run:
```bash
cat /proc/oslab_ringbuf
```
Expected: `total_written: 400`、`total_read: 400`；`read_block_count` 与 `write_block_count` 至少有一个 > 0（小缓冲下读/写曾阻塞）。

- [ ] **Step 6: 卸载并提交**

```bash
sudo rmmod oslab_ringbuf
git add extension/char_driver/pc_demo.c
git commit -m "test(driver): fork-based producer/consumer concurrency demo"
```

---

## Task 6: 加载脚本 + README + 完整验证

补齐便捷脚本与文档，跑一次 `make test` 全流程，并检查卸载无泄漏。

**Files:**
- Create: `extension/char_driver/load.sh`
- Create: `extension/char_driver/unload.sh`
- Create: `extension/char_driver/README.md`

- [ ] **Step 1: 写 load.sh**

Create `extension/char_driver/load.sh`:
```sh
#!/bin/sh
set -e
MOD=oslab_ringbuf
BUF=${1:-1024}
sudo rmmod "$MOD" 2>/dev/null || true
sudo insmod "${MOD}.ko" buf_size="${BUF}"
sudo chmod 666 "/dev/${MOD}"
echo "loaded ${MOD} (buf_size=${BUF}); /dev/${MOD} ready"
```

- [ ] **Step 2: 写 unload.sh**

Create `extension/char_driver/unload.sh`:
```sh
#!/bin/sh
set -e
sudo rmmod oslab_ringbuf
echo "unloaded oslab_ringbuf"
```

- [ ] **Step 3: 赋可执行权限**

Run:
```bash
chmod +x load.sh unload.sh
```
Expected: 无输出，`ls -l load.sh unload.sh` 显示 `x` 位。

- [ ] **Step 4: 写 README.md**

Create `extension/char_driver/README.md`:
````markdown
# oslab_ringbuf — 内核态环形缓冲区字符设备驱动

OS 课程设计 3.2 扩展部分。把基础部分的"用户态信号量生产者-消费者"升级为真实的
Linux 字符设备驱动：内核维护环形缓冲区，用户进程经 read/write 充当生产者/消费者，
内核 mutex + 等待队列实现阻塞同步，`/proc/oslab_ringbuf` 导出统计。

## 环境

- Linux，内核 5.6+（在 `uname -r` = <填写你的版本> 上验证）
- 安装头文件：`sudo apt install build-essential linux-headers-$(uname -r)`

## 构建

```bash
make            # 编译内核模块 + 用户态程序
```

## 加载 / 卸载

```bash
./load.sh           # 默认 buf_size=1024
./load.sh 16        # 指定缓冲区大小(字节)
./unload.sh
```

## 测试

```bash
make test           # 自动: 加载(buf_size=16) -> test_ringbuf -> pc_demo -> 读 /proc -> 卸载
```

- `test_ringbuf`：单进程写读一致性 + O_NONBLOCK 空读返回 EAGAIN。
- `pc_demo`：fork 生产者/消费者，小缓冲逼出阻塞，校验不丢不重。

## /proc 接口

```bash
cat /proc/oslab_ringbuf        # 查看统计
echo reset | sudo tee /proc/oslab_ringbuf   # 清零统计
```

## 设计要点（报告用）

- 设备注册：alloc_chrdev_region + cdev + class_create/device_create（自动建 /dev 节点）。
- 同步：一把 mutex 保护缓冲与计数；read_wq/write_wq 两个等待队列实现空读/满写阻塞，
  读后唤醒写者、写后唤醒读者。
- 边界：copy_to/from_user 失败返回 -EFAULT；O_NONBLOCK 返回 -EAGAIN；信号中断返回 -ERESTARTSYS。
- 与基础部分对照：用户态信号量模拟 → 内核态真实同步实现。
````

将 README 中 `<填写你的版本>` 替换为 `uname -r` 的实际输出。

- [ ] **Step 5: 跑完整 make test 全流程**

Run:
```bash
make clean && make test
```
Expected: 依次打印 `test_ringbuf` 全部通过、`pc_demo: PASS`、`/proc/oslab_ringbuf` 统计，末尾 `=== ALL TESTS DONE ===`，全程无报错。

- [ ] **Step 6: 检查卸载无泄漏 / 告警**

Run:
```bash
sudo dmesg | tail -n 15
lsmod | grep oslab_ringbuf || echo "module not loaded (clean)"
ls /dev/oslab_ringbuf /proc/oslab_ringbuf 2>&1
```
Expected: dmesg 有成对的 loaded/unloaded，无 oops / warning / "leaked" 字样；模块未驻留；`/dev` 与 `/proc` 节点均不存在。

- [ ] **Step 7: 提交**

```bash
git add extension/char_driver/load.sh extension/char_driver/unload.sh extension/char_driver/README.md
git commit -m "docs(driver): load/unload scripts, README, full make test flow"
```

---

## 完成后

所有任务完成且 `make test` 通过后，使用 **superpowers:finishing-a-development-branch** 收尾（验证测试、选择合并 / PR / 清理）。

报告素材清单：`uname -r`、dmesg loaded/unloaded 截图、`make test` 输出、`/proc/oslab_ringbuf` 输出、pc_demo PASS 输出、reset 前后对比。
```
