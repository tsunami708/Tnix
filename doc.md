## 系统启动
`kernel/boot/entry.S`

entry是内核的入口，qemu会将内核文件加载到固定物理地址0x80000000处。通过链接脚本确保entry地址为0x80000000。
内核很快就会执行由C编写的逻辑，就会涉及到函数栈帧，在初始阶段每一个CPU的栈指针是未知的。因此entry的任务是为每一个CPU分配一个4KB的栈页并初始化栈指针sp。*(entry必须是一个汇编函数以确保不会进行任何栈读写操作)*

`kernel/boot/start.c`

CPU执行完entry后跳转到start函数。
start完成4件事:
1. 设置tp寄存器为当前核信息结构的地址 *(tp -> struct cpu)*
2. 设置mstatus,mepc寄存器为进入监管模式做准备
3. 委托中断与异常处理至监管模式
4. 定时器初始化

```c
__attribute__((aligned(16))) char cpu_stack[PGSIZE * NCPU];
struct cpu cpus[NCPU];
```
cpus是静态分配的核信息结构数组，i号核的tp值即为cpus+i。当前内核最多仅支持NCPU个核心。
cpu_stack是每个CPU的初始栈，它必须是16字节对齐的，这是RISC-V的硬件要求。

`kernel/boot/main.c`

start执行mret后跳转到main函数，main的任务是完成一系列的资源初始化。初始化的任务基本上由0号核完成，其他核心在0号核准备完之前自旋等待。
0号核通过设置变量**cpu_ok**为真以向其他核心通知初始化已完成。

所有核初始化完成后开启中断 *(sti)* 并进入任务调度 *(task_schedule)*

## 并发控制
### 自旋锁
`kernel/util/spinlock.h kernel/util/spinlock.c`
```c
struct spinlock {
  const char* lname;
  bool locked;
  struct cpu* cpu;
};
struct cpu {
  //...
  bool raw_intr; 
  u8 spinlevel;  
  //...
};
```
Tnix的自旋锁是非递归的，因此需要在申请自锁时关闭本地中断避免在临界区中发生中断导致死锁。
spin_get通过push_intr而非cli是无法保证每一次获取自选锁之前当前核是否已经持有其他自旋锁，是否出于开中断状态。
push_intr会记录当前核从无锁状态到有锁状态时刻的中断状态 *(raw_intr)* ，并记录当前核持有的自旋锁个数 *(spinlevel)*。
spin_put调用pop_intr。pop_intr减一spinlevel，当其变为0时根据raw_intr判断是否开启中断。

### 阻塞锁
`kernel/util/sleeplock.h kernel/util/sleeplock.c`
```c
struct sleeplock {
  const char* lname;
  bool locked;
  struct task* task;
};
```
**自旋锁面向CPU的，而阻塞锁面向task。**
不能在自旋锁的临界区内申请阻塞锁，因为阻塞锁可能会导致当前线程被切换走但x号核依旧持有自旋锁，线程被唤醒时可能由y号核继续执行，y号核会执行后续的spin_put尝试释放一个未持有的自旋锁导致panic。

## 内存管理
```c
struct page { // 物理4KB页
  struct list_node page_node;
  u64 paddr;
  bool inuse;
};
```

`kernel/mem/alloc.h kernel/mem/alloc.c`

- init_memory

Tnix通过一个大链表管理所有的4KB空闲物理页面。链表是静态定义的，在init_memory中会将所有的页面通过前驱和后继指针级联。被分配走的物理页会从链表中移除，分配物理页时只需要返回表头即可。

- alloc_page_for_task | free_page_for_task
```c
struct task{
  //...
  struct list_node pages; // 进程私有物理页面链表
  //...
}
```
这2个函数是对alloc_page与free_page的简单封装，Tnix每一个进程都拥有一个存储私有物理页的链表从而方便进程结束时内核回收。
alloc_page_for_task 和 free_page_for_task 在申请或释放物理页的同时将物理页加入或删除于进程私有物理页面链表。

### 虚拟地址
`kernel/mem/vm.h kernel/mem/vm.c`

- init_page | svmmap

Tnix使用SV39分页。

svmmap用于构建虚拟页与物理页的地址，它既可以用于进程，也能用于内核自身。init_page的核心任务是初始化内核页表，使得内核虚拟地址一一映射到值相等的物理地址。
**Tnix没有实现大页映射，mvmmap是为了加速内核构建一一映射而编写的，它只能在init_page中用于内核页表**

![地址空间映射](https://i-blog.csdnimg.cn/direct/128cd7d1f20f4ca4b8582b0352986957.png)


## 中断与异常
*中断与异常统称为陷阱*

`kernel/trap/trap.c kernel/trap/pt_reg.S`

- ktrap_entry | utrap_entry

RSIC-V在陷阱触发时会跳转的stvec所指向的地址执行陷阱处理逻辑。
在监管模式时stvec指向ktrap_entry，用户模式时stvec指向utrap_entry。
ktrap_entry和utrap_entry都必须处理的是保存与恢复陷阱上下文，utrap_entry由于涉及到特权级的切换，还需要多出一步页表切换的步骤。

- do_trap

do_trap是ktrap_entry和utrap_entry在保存完陷阱上下文后调用的函数。它根据读取控制寄存器判断陷阱属于中断还是异常，进一步判断具体的中断或异常，更具中断异常向量表执行具体的陷阱处理函数。
Tnix只简单实现了时钟中断，外部中断中的终端/硬盘中断以及用户系统调用。对于其他陷阱均做panic处理。


`kernel/trap/pt_reg.h`
```c
struct pt_regs { //陷阱上下文
  u64 ra, sp, gp, tp;
  u64 sepc, sstatus, stval, scause;
  u64 t0, t1, t2, t3, t4, t5, t6;
  u64 s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11;
  u64 a0, a1, a2, a3, a4, a5, a6, a7;
};
```

陷阱上下文包含所有的通用寄存器和4个陷阱相关的控制寄存器。由于陷阱的触发时间是未知的，陷阱处理函数总是被突然执行，没有像普通函数一样存在严格的caller-callee关系，不能够指望调用者保存部分寄存器。
**陷阱上下文中每一个字段的声明顺序不能随意更改，因为ktrap_entry和utrap_entry汇编函数在保存与恢复陷阱上下文时是根据字段地址偏移约定读写数据的。**

### trampoline页
```assemble
#...
  sfence.vma zero, zero
  li sp, TRAPFRAME
  ld sp, 0(sp) #获取内核页表
  csrw satp, sp
  sfence.vma zero, zero
#...
  sfence.vma zero, zero
  ld sp, TASK_USATP(tp)
  csrw satp, sp #获取进程页表
  sfence.vma zero, zero
#...
```
trampoline是一个对应2块虚拟页的特殊页面，用于用户陷入内核时的页表安全切换,它被内核和所有进程共享 *(本质是一个代码段)*。trampoline的高地址映射对于内核和进程是相等的。

#### trapframe页
trapframe页用于保存陷阱上下文需要用到数据，目前仅保存一个内核页表地址。它目前是进程私有的，并且总是被固定映射到进程地址空间的高地址处。

## 任务调度
### CPU上下文
`kernel/task/cpu.h kernel/task/switch.S`
```c
struct context {
  u64 ra, sp;
  u64 s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11;
};
```
CPU上下文不同于陷阱上下文，它在context_switch中被切换，而context_switch是**C函数**同步调用的，存在严格的caller-callee关系，因此在context_switch只需要保存被调用者需保存的通用寄存器即可。

### RR调度器
`kernel/task/sche.h kernel/task/sche.c`

Tnix使用了最简单的时间片轮转调度算法，核心函数是task_schedule，它通过不断轮询调度READY状态的线程。每一个线程在时间片到期或时间等待时让出CPU资源。

`kernel/task/cpu.h`

```c
struct cpu {
  //...
  struct context ctx; 
};
```
为了方便线程让出CPU后当前核能返回task_schedule选择下一个线程，每个核也存储了自身的调度器上下文。 *(struct cpu : ctx)*

某个核改变一个线程状态，随后执行context_switch，这个过程必须要保证处于临界区中，随后由context_switch跳转到的逻辑释放锁。

*eg: task_schedule~yield*
![task_schedule~yield](https://i-blog.csdnimg.cn/direct/2ab8c7f5bd504110aea5b910e2e0e3f9.png)

*对于task_schedule~sleep task_schedule~kill task_schedule~sys_fork task_schedule~first_sched task_schedule~sys_exec task_schedule~exit也是同理*


## 文件系统
`kernel/fs/fs.h kernel/fs/fs.c`

![文件系统](https://i-blog.csdnimg.cn/direct/3a913b4cbbfd4d39bce62acb2b6e3111.png)

### 目录文件
`kernel/fs/dir.h kernel/fs/dir.c`
```c
struct dentry {
  u32 inum;
  char name[DLENGTH] __attribute__((nonstring));
};
```
![目录](https://i-blog.csdnimg.cn/direct/d17f7d78fc3c4bcdba9185edb8ca1663.png)

