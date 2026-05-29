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
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/wait.h>

#define DEVICE_NAME "oslab_ringbuf"
#define PROC_NAME "oslab_ringbuf"

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
static DECLARE_WAIT_QUEUE_HEAD(read_wq);
static DECLARE_WAIT_QUEUE_HEAD(write_wq);

/* statistics (protected by rb_lock) */
static unsigned long total_written;
static unsigned long total_read;
static unsigned long open_count;
static unsigned long read_block_count;
static unsigned long write_block_count;

static int rb_open(struct inode *inode, struct file *filp)
{
    mutex_lock(&rb_lock);
    open_count++;
    mutex_unlock(&rb_lock);
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
    total_read += to_copy;
    ret = to_copy;
    wake_up_interruptible(&write_wq);
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
    total_written += to_copy;
    ret = to_copy;
    wake_up_interruptible(&read_wq);
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

static const struct proc_ops rb_proc_ops = {
    .proc_open = rb_proc_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
    .proc_write = rb_proc_write,
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

    if (!proc_create(PROC_NAME, 0666, NULL, &rb_proc_ops)) {
        ret = -ENOMEM;
        goto err_device;
    }

    pr_info("oslab_ringbuf: loaded, major=%d buf_size=%d\n",
            MAJOR(dev_num), buf_size);
    return 0;

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
}

static void __exit rb_exit(void)
{
    remove_proc_entry(PROC_NAME, NULL);
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
