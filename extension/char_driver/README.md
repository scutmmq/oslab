# oslab_ringbuf — 内核态环形缓冲区字符设备驱动

OS 课程设计 3.2 扩展部分。把基础部分的"用户态信号量生产者-消费者"升级为真实的
Linux 字符设备驱动：内核维护环形缓冲区，用户进程经 read/write 充当生产者/消费者，
内核 mutex + 等待队列实现阻塞同步，`/proc/oslab_ringbuf` 导出统计。

## 环境

- Linux，内核 5.6+（在 `uname -r` = 6.17.0-20-generic / Ubuntu 24.04.3 LTS 上验证通过）
- 安装头文件与工具：`sudo apt install build-essential linux-headers-$(uname -r)`

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
echo reset > /proc/oslab_ringbuf   # 清零统计
```

## 设计要点（报告用）

- 设备注册：alloc_chrdev_region + cdev + class_create/device_create（自动建 /dev 节点）。
- 同步：一把 mutex 保护缓冲与计数；read_wq/write_wq 两个等待队列实现空读/满写阻塞，
  读后唤醒写者、写后唤醒读者。
- 边界：copy_to/from_user 失败返回 -EFAULT；O_NONBLOCK 返回 -EAGAIN；信号中断返回 -ERESTARTSYS。
- 与基础部分对照：用户态信号量模拟 → 内核态真实同步实现。
