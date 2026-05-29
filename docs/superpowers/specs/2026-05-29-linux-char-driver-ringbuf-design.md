# 设计文档：Linux 内核态环形缓冲区字符设备驱动 + /proc 接口

- 日期：2026-05-29
- 所属：OS 课程设计 3.2 自由扩展提升部分（占 30%）
- 截止：2026-06-11
- 状态：已通过设计评审，待转实现计划

## 1. 背景与立意

基础部分（3.1）已用**用户态 pthread + 信号量**模拟了生产者-消费者问题。本扩展将其"做真"：实现一个真正的 Linux 字符设备驱动，内核中维护一个环形缓冲区，多个用户进程通过 `write()` / `read()` 系统调用充当生产者 / 消费者，使用**真实的内核同步原语**（`mutex` + 等待队列 `wait_queue`）实现阻塞读写并杜绝数据竞争。

这条主线形成"用户态模拟 → 内核态实现"的递进叙事，直接覆盖需求文档中的：
- 系统级编程能力（Linux 内核模块、字符设备驱动）
- 并发控制（互斥锁、阻塞同步、避免数据竞争）
- `/proc` 接口开发

## 2. 范围

### 做什么
- 一个可加载的 Linux 内核模块，注册字符设备 `/dev/oslab_ringbuf`。
- 内核态固定大小环形字节缓冲区，支持阻塞 / 非阻塞读写。
- 内核同步：互斥 + 读写等待队列。
- `/proc/oslab_ringbuf`：读出运行统计；写入 `reset` 清零统计。
- 用户态测试程序：正确性断言测试 + fork 并发演示。
- kbuild 构建系统、加载 / 卸载脚本、README。

### 不做什么（YAGNI）
- 不做定长消息队列语义（采用通用字节流环形缓冲）。
- 不做多设备实例 / 多次设备号。
- 不做 ioctl 配置接口（缓冲区大小用模块参数即可）。
- 不触碰现有 Windows/CMake 工程，扩展完全独立。

## 3. 目录结构

新增独立子目录，不影响现有工程：

```
extension/char_driver/
  oslab_ringbuf.c     内核模块源码
  Makefile            kbuild 构建 + 便捷目标 (build/load/unload/test/clean)
  test_ringbuf.c      正确性测试：写入读回断言、O_NONBLOCK 测试
  pc_demo.c           fork 生产者+消费者进程，演示阻塞与并发不丢不重
  load.sh             insmod + chmod 666 /dev 节点
  unload.sh           rmmod
  README.md           Linux 上构建/加载/测试步骤 + 报告素材说明
```

## 4. 内核模块设计（oslab_ringbuf.c）

### 4.1 设备注册
- `alloc_chrdev_region` 动态申请主设备号。
- `cdev_init` + `cdev_add` 绑定 `file_operations`。
- `class_create` + `device_create` 自动生成 `/dev/oslab_ringbuf`（经 udev）。
- 模块参数：`buf_size`（默认 1024 字节），通过 `module_param` 暴露。

### 4.2 环形缓冲区状态
```
static char  *buffer;        // kmalloc 分配, 大小 buf_size
static size_t head;          // 写入位置
static size_t tail;          // 读取位置
static size_t count;         // 当前占用字节数
```
- 满：`count == buf_size`；空：`count == 0`。

### 4.3 同步
- `static DEFINE_MUTEX(lock);` 保护缓冲区与计数。
- `static DECLARE_WAIT_QUEUE_HEAD(read_wq);`
- `static DECLARE_WAIT_QUEUE_HEAD(write_wq);`
- 读：缓冲区空时 `wait_event_interruptible(read_wq, count > 0 || ...)` 阻塞；读出后 `wake_up_interruptible(&write_wq)`。
- 写：缓冲区满时阻塞于 `write_wq`；写入后 `wake_up_interruptible(&read_wq)`。
- 等待前后正确释放 / 重获 mutex，避免持锁睡眠。

### 4.4 file_operations
| 回调 | 行为 |
|------|------|
| `open` | 递增 open 计数；记录 dmesg |
| `release` | 记录 dmesg |
| `read` | 持锁；空且阻塞模式→睡眠，空且 `O_NONBLOCK`→`-EAGAIN`；`copy_to_user`，失败 `-EFAULT`；返回实际字节数；唤醒写者 |
| `write` | 持锁；满且阻塞模式→睡眠，满且 `O_NONBLOCK`→`-EAGAIN`；`copy_from_user`，失败 `-EFAULT`；返回实际字节数；唤醒读者 |
- 信号中断睡眠返回 `-ERESTARTSYS`。
- 支持部分读写（请求长度大于可用空间时尽力而为）。

### 4.5 统计计数
```
total_written, total_read   // 累计字节
open_count                  // 打开次数
read_block_count            // 读阻塞发生次数
write_block_count           // 写阻塞发生次数
// 当前占用 = count
```

### 4.6 /proc 接口
- `proc_create("oslab_ringbuf", 0666, NULL, &proc_ops)`（使用内核 5.6+ 的 `struct proc_ops`）。
- 读：格式化输出缓冲区大小、当前占用、累计写 / 读、打开次数、读 / 写阻塞次数。
- 写：内容为 `reset` 时清零各计数（当前占用不清，因属缓冲区真实状态）。

### 4.7 生命周期与健壮性
- `module_init`：goto 逐级回滚（按 region→cdev→class→device→buffer→proc 顺序申请，失败逆序释放）。
- `module_exit`：逆序清理（remove proc→device_destroy→class_destroy→cdev_del→unregister_chrdev_region→kfree）。
- 关键路径写 `pr_info` / `pr_err` 便于 dmesg 观察。

## 5. 用户态程序设计

### 5.1 test_ringbuf.c（正确性）
- 打开 `/dev/oslab_ringbuf`，写入已知字符串，读回并 `assert` 内容一致、返回字节数正确。
- 以 `O_NONBLOCK` 打开，空缓冲读应返回 -1 且 `errno == EAGAIN`。
- 退出码非 0 表示失败，便于脚本判定。

### 5.2 pc_demo.c（并发）
- `fork` 出生产者子进程（写入 N 条带序号的数据）和消费者子进程（读取并校验）。
- 故意使用较小 `buf_size`，逼出写满阻塞 / 读空阻塞。
- 消费者统计收到的条目，验证**每条恰好消费一次，不丢不重**。

### 5.3 统计查看
- `cat /proc/oslab_ringbuf` 查看运行统计。
- `echo reset > /proc/oslab_ringbuf` 清零计数。

## 6. 数据流

```
生产者进程 write() → 驱动 .write →（持锁; 满则睡眠于 write_wq）
                     copy_from_user 入环形区 → wake_up read_wq
消费者进程 read()  → 驱动 .read  →（持锁; 空则睡眠于 read_wq）
                     copy_to_user 出环形区 → wake_up write_wq
/proc 读 → .proc_read 格式化统计计数输出
/proc 写 "reset" → 清零计数
```

## 7. 构建系统（Makefile）

- kbuild 模块编译：
  ```
  obj-m += oslab_ringbuf.o
  KDIR := /lib/modules/$(shell uname -r)/build
  modules: ; $(MAKE) -C $(KDIR) M=$(PWD) modules
  ```
- 便捷目标：`build`（模块+用户程序）、`load`（=load.sh）、`unload`、`test`（加载→跑 test_ringbuf 与 pc_demo→读 proc→卸载）、`clean`。
- 用户态程序用 `gcc -Wall -Wextra`。

## 8. 测试策略

| 层次 | 验证点 |
|------|--------|
| 加载 | insmod 成功；dmesg 正常；`/dev/oslab_ringbuf` 与 `/proc/oslab_ringbuf` 存在 |
| 正确性 | 写入读回内容一致、字节数正确（test_ringbuf 断言） |
| 非阻塞 | `O_NONBLOCK` 空读返回 EAGAIN |
| 并发 | pc_demo：小缓冲逼出阻塞，生产数据被消费恰好一次，不丢不重 |
| 压力 | 写入远大于缓冲区的数据，循环多次，无丢失 |
| 统计 | /proc 输出与实际读写字节吻合；reset 生效 |
| 卸载 | rmmod 后 /dev 与 /proc 节点消失；dmesg 无泄漏 / 告警 |

## 9. 风险与适配

- **内核版本差异**（实现计划首步检测 `uname -r`）：
  - `proc_create` 在 5.6+ 使用 `struct proc_ops`（旧版为 `file_operations`）。本设计目标 5.6+。
  - `class_create` 在 6.4+ 去掉 `owner` 参数。
  - README 注明实测内核版本与适配点。
- 加载与测试需 `sudo`；load.sh 对设备节点 `chmod 666` 以便非 root 用户态程序访问。
- 完全独立于现有 Windows/CMake 工程，互不影响；本扩展仅在 Linux 环境构建运行。

## 10. 交付物（报告素材）

- 内核模块源码、Makefile、用户态测试程序、加载脚本、README。
- dmesg 日志、`/proc/oslab_ringbuf` 输出、并发演示输出（截图入报告）。
- 报告论述："用户态信号量模拟（基础部分）→ 内核态真实同步实现（扩展部分）"的对比与原理分析。
