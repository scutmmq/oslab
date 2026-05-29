# 操作系统课程设计

> 计算机科学与技术 1 班　姓名：______　学号：______
> 代码仓库：https://github.com/scutmmq/oslab

这是我的《操作系统》课程设计。按照课程要求文档 `doc/操作系统课程设计内容与要求1.pdf`，我完成了 **3.1 基础必做** 的全部四个模块，并在此基础上完成了 **3.2 自由扩展提升** 部分——用 C 语言实现了一个真实的 Linux 字符设备驱动。

## 我完成的内容

### 3.1 基础必做（控制台程序，纯 C 实现）

1. **处理机调度**
   - FCFS 先来先服务、SJF 非抢占式短作业优先、RR 时间片轮转、非抢占式优先级调度（优先级数值越小越高）
   - 支持动态输入进程参数，输出运行顺序、完成/周转/等待/响应时间及平均值
   - 额外做了**四种算法一键对比**功能，方便分析不同算法的性能差异

2. **内存管理**
   - FIFO / LRU 页面置换，统计缺页次数与缺页率，展示每一步的内存块状态
   - 首次适应 / 最佳适应动态分区分配，展示分配、回收与相邻空闲分区合并

3. **进程同步与并发控制**
   - 生产者-消费者、读者-写者、哲学家进餐三个经典问题
   - 用 `pthread` + 互斥锁 + 信号量实现并发模拟，哲学家问题用按序拿叉子避免死锁

4. **文件系统**
   - 简易块式文件系统，支持创建、写入、读取、删除、列目录、格式化
   - 用空闲块位图管理存储空间

### 3.2 自由扩展提升：Linux 内核字符设备驱动

我把基础部分"用户态信号量模拟的生产者-消费者"进一步做成了**真实的内核实现**：一个字符设备驱动 `/dev/oslab_ringbuf`，内核里维护环形缓冲区，用户进程通过 `read`/`write` 充当生产者/消费者，用**内核 mutex + 等待队列**实现阻塞同步，并通过 `/proc/oslab_ringbuf` 导出运行统计。

- 设备注册（cdev + class，自动创建设备节点）、阻塞与 `O_NONBLOCK` 读写、`/proc` 统计与 reset
- 配套用户态测试程序（正确性断言 + fork 并发演示）、构建与加载脚本
- 已在 **Ubuntu 24.04 / 内核 6.17** 虚拟机上实测通过（`make test` 全绿，并发演示不丢不重）

详见 [extension/char_driver/README.md](extension/char_driver/README.md)。

## 目录结构

```text
include/oslab/        基础部分对外头文件
src/                  基础部分功能实现与主程序
tests/                基础部分核心算法测试
extension/char_driver/  3.2 扩展：Linux 字符设备驱动（仅在 Linux 下构建运行）
doc/                  课程设计要求文档
docs/                 我的设计文档与实现计划
CMakeLists.txt        基础部分 CMake 构建配置
```

## 基础部分：构建与运行

推荐使用 CMake（Windows / Linux 通用）：

```bash
cmake -S . -B build
cmake --build build
./build/oslab_app          # Windows 下为 .\build\oslab_app.exe
```

也可以直接用 GCC：

```bash
gcc -std=c11 -Wall -Wextra -Iinclude src/main.c src/scheduler.c src/memory.c src/filesystem.c src/sync_demo.c -o oslab_app -lpthread
./oslab_app
```

程序是菜单式交互，启动后输入数字选择模块，进入各模块后按屏幕提示输入参数（每个模块都带了用法说明和示例）。

## 基础部分：测试

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

测试覆盖：FCFS/SJF/RR 的运行顺序与时间统计、FIFO/LRU 的缺页统计、首次/最佳适应的分配与回收合并、文件系统的创建/写/读/删与空闲块回收。

## 扩展部分：使用

扩展部分是 Linux 内核模块，**只能在 Linux 上构建运行**：

```bash
cd extension/char_driver
make              # 编译内核模块 + 用户态程序
./load.sh         # 加载驱动（需 sudo）
make test         # 一键测试：加载 → 正确性测试 → 并发演示 → 读 /proc → 卸载
./unload.sh       # 卸载
```

手动体验：

```bash
echo "hello kernel" > /dev/oslab_ringbuf   # 写入（生产者）
cat /dev/oslab_ringbuf                      # 读出（消费者）
cat /proc/oslab_ringbuf                     # 查看统计
```

## 开发环境说明

- 基础部分在 Windows（MinGW）与 Linux 下均可用 CMake 构建，代码可移植（Windows 相关代码用 `#ifdef _WIN32` 隔离）。
- 扩展部分在 Ubuntu 24.04 LTS（内核 6.17）虚拟机上开发与验证，需安装内核头文件：
  `sudo apt install build-essential linux-headers-$(uname -r)`
