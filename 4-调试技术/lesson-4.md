# 调试技术

## 内核中的调试支持 *（Debugging Support in the Kernel）*

- 内核编程带有独特的调试难题。因为内核是一个不与特定进程相关连的功能的集合，内核代码无法轻易地在一个调试器下运行, 也很难被跟踪。 内核代码错误也特别难以重现, 它们会牵连整个系统崩溃，从而破坏了大量的能用来追踪错误的证据。一句话，内核编程的调试相对于应用程序来说比较困难。
- 书中建议我们安装一个自己构造的内核，而不是直接使用发行版的系统，主要原因是内核开发者在内核中构建了多个调试的功能，但是这些功能会造成额外的输出，降低性能，因此在发行版中会关闭这些功能。这些功能的选项均出现在内核配置工具 `"kernel hacking"` 菜单中，注意，并非所有体系架构都支持其中的某些选项。

## 通过打印调试 *（Debugging by Printing）*

- 最普通的调试技术是监视，在应用程序编程当中是通过在合适的地方调用 `printf` 来实现。 在你调试内核代码时，你可以通过 printk 来达到这个目的。

### printk  *（printk）*

- `printk`与`printf`的区别：
  - 首先就是 printk 是内核实现的，不是由 libc 库之类实现的，它主要的工作范围是内核。
  - 通过附加不同的日志级别（loglevel 消息优先级），可以让`printk`根据这些优先级所表示的严重程度对消息进行分类。主要通过宏来指定日志级别，而宏会展开称为一个字符串，在编译时由预处理器将它和消息文本拼接到一起，这也就是为什么优先级宏与格式字符串间没有 逗号`,` 。**每个字符串（以宏的形式展开）代表一个尖括号中的整数。整数值的范围从0到7，数值越小，优先级就越高。**
  - 在头文件 `<linux/kernel.h>` 里定义了八种可用的日志级别字符串;
     1. `KERN_EMERG`：用于紧急消息, 常常是那些崩溃前的消息.
     2. `KERN_ALERT`：需要立刻采取动作的情况.
     3. `KERN_CRIT`：临界情况，常常与严重的硬件或者软件操作失效有关.
     4. `KERN_ERR`：用来报告错误情况，设备驱动常常使用 KERN_ERR 来报告硬件故障.
     5. `KERN_WARNING`：对可能出现问题的情况进行警告, 这些情况通常不会对系统造成严重问题.
     6. `KERN_NOTICE`：需要提醒的正常情况，许多与安全相关的情况用这个级别进行汇报.
     7. `KERN_INFO`：提示性消息， 很多驱动在启动时以这个级别打印它们发现的硬件的信息.
     8. `KERN_DEBUG`：用作调试消息.

### 重定向到控制台 *（Redirecting Console Messages）*

- 内核可以讲消息发送到一个指定的虚拟控制台（假如控制台是文本屏幕的话）。默认情况下，**“控制台”就是当前地虚拟终端**。可以在任何一个控制台设备上调用 ioctl（TIOCLINUX），**来指定接收消息的虚拟终端**。下面的 `setconsole` 程序，可选择专门用来接收内核消息的控制台；这个程序必须由超级用户运行，在 misc-progs 目录里可以找到它。下面是程序的代码：

```c
int main(int argc, char **argv)
{
        char bytes[2] = {11,0}; /* 11 is the TIOCLINUX cmd number */
        if (argc==2)
            bytes[1] = atoi(argv[1]); /* the chosen console */
        else {
              fprintf(stderr, "%s: need a single arg\n",argv[0]); exit(1);
        }
        if (ioctl(STDIN_FILENO, TIOCLINUX, bytes)<0) { /* use stdin */
              fprintf(stderr,"%s: ioctl(stdin, TIOCLINUX): %s\n", argv[0], strerror(errno));
              exit(1);
        }
        exit(0);
}
```

### 消息如何被记录 *（How Messages Get Logged）*

1. `printk` 函数将消息写到一个长度为 `__LOG_BUF_LEN`（定义在 `kernel/printk.c` 中）字节的`ring buffer`中去的，然后唤醒任何正在等待消息的进程，即那些睡眠在 `syslog` 系统调用上的进程，或者读取 `/proc/kmesg` 的进程。`dmesg`所显示的内容也是从 ring buffer 中读取的。**如果循环缓冲区填满了，printk就绕回缓冲区的开始处填写新数据，覆盖最陈旧的数据，于是记录进程就会丢失最早的数据。**

2. LINUX系统启动后，由`/etc/init.d/sysklogd`先后启动`klogd`，`syslogd`两个守护进程。
    1. `klogd`会通过`syslog()`系统调用或者读取`proc`文件系统（/proc/kmsg）来从系统缓冲区(ring buffer)中得到由内核`printk()`发出的信息
    2. `klogd`的输出结果会传送给`syslogd`进行处理，`syslogd`会根据`/etc/syslog.conf`的配置把`log`信息输出到`/var/log/`下的不同文件中
    3. 当printk指定的优先级（`DEFAULT_MESSAGE_LEVEL 4`）小于指定的控制台优先级console_logleve（DEFAULT_CONSOLE_LOGLEVEL 7）l时，调试消息就显示在控制台虚拟终端，修改当前控制台优先级可以使用:dmesg -n x，kmsg -c x（重启生效），修改/proc/sys/kernel/printk
    4. printk的输出会被输出到默认console上，比如启动的时候将串口指定为默认的console后，就会输出到串口

## 通过查询调试 *（Debugging by Querying）*

- 由于 syslogd 会一直保持对其**输出文件的同步刷新**，每打印**一行**都会引起**一次磁盘操作**，因此大量使用 printk **会严重降低系统性能**。
- 多数情况中，**获取相关信息**的最好方法是在**需要的时候才去查询系统信息**，而不是持续不断地产生数据。实际上，每个 Unix 系统都提供了很多工具，用于获取系统信息，如：ps、netstat、vmstat等等。
- 驱动程序开发人员对系统进行查询时，可以采用两种主要的技术：**在 /proc 文件系统中创建文件，或者使用驱动程序的 ioctl 方法**。/proc 方式的另一个选择是使用 devfs，不过用于信息查找时，/proc 更为简单一些。
- /proc 文件不仅仅用于读取，也可以进行写入，但是现在并不推荐在 /proc 下添加新的文件，相比最开始的目的，/proc文件系统变得太臃肿了。因此建议使用新的 sysfs 文件系统来取代它。但是/proc 依旧是简单便于使用。

### 使用 /proc 文件系统 *（Using the /proc Filesystem）*

- 想要使用 proc 文件系统首先要包含 <linux/proc_fs.h>头文件。
- 为创建一个**只读** /proc 文件，驱动程序必须**实现一个函数**，用于在文件读取时**生成数据**。当某个进程读这个文件时（使用 read 系统调用），请求会通过这个函数发送到驱动程序模块，我们把注册接口放到本节后面，先讲函数。无论采用哪个接口，在这两种情况下，内核都会分配一页内存（也就是 `PAGE_SIZE`个字节），驱动程序向这片内存写入将返回给用户空间的数据。推荐的接口是 `read_proc`，不过还有一个名为 `get_info` 的老一点的接口。
- 创建的过程：
    1. proc_create创建文件
    2. create 函数的一个主要参数就是 proc_fops 就是该文件的操作函数了，定义了如何操作文件的方式。
- 因为 /proc 下大文件的实现有些笨拙，为了方便开发，增加了 `seq_file` 接口，这个接口为大的内核虚拟文件提供了一组更加简单的函数。因此后续的源码中 /proc 的大多是通过 seq_file 来实现的。

#### 使用seq_file接口 *（Using the seq_file API）*

- `seq_file` 的实现基于 /proc 系统。要使用 seq_file，我们必须抽象出一个对象序列，然后可以依次遍历对象序列的每个成员。这个对象序列可以是链表，数组，哈希表等等。具体到scull模块，是把scull_devices 数组做为一个对象序列，每个对象就是一个 `scull_dev`  结构。

- seq_file 接口有两个重要数据结构：

```c
struct seq_file {
    char *buf;
    size_t size;
    size_t from;
    size_t count;
    loff_t index;
    loff_t read_pos;
    u64 version;
    struct mutex lock;
    const struct seq_operations *op;
    void *private;
};
```

- seq_file 结构 在 `seq_open` 函数中创建，然后作为参数传递给每个 `seq_file` 接口操作函数。

```c
struct seq_operations {
    void * (*start) (struct seq_file *m, loff_t *pos);
    void (*stop) (struct seq_file *m, void *v);
    void * (*next) (struct seq_file *m, void *v, loff_t *pos);
    int (*show) (struct seq_file *m, void *v);
};
```

- 要使用 seq_file 接口，必须实现四个操作函数，分别是 `start()` , `next()` , `show()` , `stop()`。
  - start 函数完成初始化工作，在遍历操作开始时调用，返回一个对象指针。
  - show 函数对当前正在遍历的对象进行操作，利用 seq_printf，seq_puts等 函数，打印这个对象的信息。
  - next 函数在遍历中寻找下一个对象并返回。
  - stop 函数在遍历结束时调用，完成一些清理工作。

- 下面我们看scull模块中是怎样使用seq_file接口的：

```c
/*
* For now, the seq_file implementation will exist in parallel.  The
* older read_procmem function should maybe go away, though.
*/
/*
* Here are our sequence iteration methods.  Our "position" is
* simply the device number.
*/
static void *scull_seq_start(struct seq_file *s, loff_t *pos)
{
    if (*pos >= scull_nr_devs)
        return NULL;  /* No more to read */return scull_devices + *pos;
}

static void *scull_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
    (*pos)++;
    if (*pos >= scull_nr_devs)
        return NULL;
    return scull_devices + *pos;
}

static void scull_seq_stop(struct seq_file *s, void *v)
{
    /* Actually, there's nothing to do here */
}
```

- 实现了scull_seq_start，scull_seq_next，scull_seq_stop三个函数，简单的返回一些数据，接下来是实现 show 函数：

```c
static int scull_seq_show(struct seq_file *s, void *v)
{
    struct scull_dev *dev = (struct scull_dev *) v;
    struct scull_qset *d;
    int i;

    if (mutex_lock_interruptible(&dev->mutex))
        return -ERESTARTSYS;
    seq_printf(s, "\nDevice %i: qset %i, q %i, sz %li\n",
            (int) (dev - scull_devices), dev->qset,
            dev->quantum, dev->size);
    for (d = dev->data; d; d = d->next) { /* scan the list */
        seq_printf(s, "  item at %p, qset at %p\n", d, d->data);
        if (d->data && !d->next) /* dump only the last item */for (i = 0; i < dev->qset; i++) {
                if (d->data[i])
                    seq_printf(s, "    % 4i: %8p\n",
                            i, d->data[i]);
            }
    }
    mutex_unlock(&dev->mutex);
    return 0;
}
```

- show 函数与前面介绍的 scull_read_procmem 逻辑是一样的，主要区别是打印语句用 seq_printf

```c
/*
* Tie the sequence operators up.
* 填充了一个seq_operations结构体，这是seq_file接口要求的。
*/
static struct seq_operations scull_seq_ops = {
    .start = scull_seq_start,
    .next  = scull_seq_next,
    .stop  = scull_seq_stop,
    .show  = scull_seq_show
};
```

接下来，我们要实现一个 file_operations 结构，这个结构将实现在该 /proc 文件上进行读取和定位时所需要的所有操作。与第三章介绍的字符设备驱动程序不同，这里我们只要实现一个 open 方法，其他的方法可以直接使用 seq_file 接口提供的函数。

```c
/* Now to implement the /proc file we need only make an open
* method which sets up the sequence operators.
* 实现了open方法scull_proc_open。
* 调用seq_open(file, &scull_seq_ops)
* 将file结构与seq_operations结构体连接在一起。
*/
static int scull_proc_open(struct inode *inode, struct file *file)
{
    return seq_open(file, &scull_seq_ops);
}

/*
* Create a set of file operations for our proc file.
* 定义了 file_operations 结构体 scull_proc_ops，
* 其中，只有 scull_proc_open 是我们自己定义的，
* 其他函数都是使用 seq_file 接口提供的函数。
*/
static struct file_operations scull_proc_ops = {
    .owner  = THIS_MODULE,
    .open    = scull_proc_open,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = seq_release
};
```

- 调用 proc_create 函数创建 /proc/scullseq 文件。接 scull_proc_ops 结构体与 /proc/scullseq 连接起来。

```c
    entry = proc_create("scullseq", 0, NULL, &scull_proc_ops);
    if (!entry) {
        printk(KERN_WARNING "proc_create scullseq failed\n");
    }
```

## 通过监视调试 *（Debugging by Watching）*

- `strace`命令是一个功能非常强大的工具，它可以显示程序所调用的**所有**系统调用。它不仅可以显示调用，而且还能显示**调用参数**，以及用符号方式表示的**返回值**。当系统调用失败时，错误的符号值（如`ENOMEM`）和对应的字符串（如`Out of memory`）都能被显示出来。
- strace 有许多命令行选项；
  - 最为有用的是 -t，用来显示调用发生的时间；
  - -T，显示调用所花费的时间；
  - -e，限定被跟踪的调用类型；
  - -o，将输出重定向到一个文件中。
  - 默认情况下，strace将跟踪信息打印到 stderr 上。

## 调试系统故障 *（Debugging System Faults）*

- Linux 代码非常健壮（用术语讲即为鲁棒，robust），可以很好地响应大部分错误：**故障通常会导致当前进程崩溃，而系统仍会继续运行**。如果在进程上下文之外发生故障，或是系统的重要组成被损害时，系统才有可能 panic。当**内核行为异常时**，会在控制台上**打印出提示信息**。

### oops消息 *（Oops Messages）*

- 大部分错误都在于 NULL 指针的使用或其他不正确的指针值的使用上。这些错误通常会导致一个 oops 消息。
- 由处理器使用的地址都是虚拟地址，而且通过一个复杂的称为页表（见第 13 章中的“页表”一节）的结构映射为物理地址。当引用一个非法指针时，页面映射机制就不能将地址映射到物理地址，此时处理器就会向操作系统发出一个**“页面失效”**的信号。如果地址非法，内核就无法**“换页”**到并不存在的地址上；如果此时处理器处于超级用户模式，系统就会产生一个`“oops”`。
- oops 显示发生错误时处理器的状态，包括 CPU 寄存器的内容、页描述符表的位置，以及其它看上去无法理解的信息。这些消息由失效处理函数（arch/*/kernel/traps.c）中的printk 语句产生，就象前面“printk”一节所介绍的那样分发出来。

### 系统挂起 *（System Hangs）*

- 尽管内核代码中的大多数错误仅会导致一个oops 消息，但有时它们则会将系统完全挂起。如果系统挂起了，任何消息都无法打印。例如，如果代码进入一个死循环，内核就会停止进行调度，系统不会再响应任何动作，包括 `Ctrl-Alt-Del`组合键。处理系统挂起有两个选择――要么是防范于未然；要么就是亡羊补牢，在发生挂起后调试代码。
- 通过在一些关键点上插入 `schedule` 调用可以防止死循环。schedule 函数（正如读者猜到的）会调用调度器，并因此允许其他进程“偷取”当然进程的CPU时间。如果该进程因驱动程序的错误而在内核空间陷入死循环，则可以在跟踪到这种情况之后，借助 schedule 调用杀掉这个进程。
