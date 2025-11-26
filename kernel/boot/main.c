#include "task/cpu.h"

volatile bool cpu_ok = false;

extern void init_memory(void);
extern void init_page(void);
extern void init_slot(void);
extern void init_trap(void);
extern void init_plic(void);
extern void init_proc1(void);
extern void init_console(void);
extern void init_bcache(void);
extern void init_icache(void);
extern void init_disk(void);
extern void task_schedule(void);


void
main(void)
{
  if (cpuid() == 0) {
    init_console(); // 终端初始化
    init_memory();  // 物理地址初始化
    init_page();    // 内核页表初始化
    init_slot();
    init_trap();   // 陷阱处理初始化
    init_plic();   // 中断控制器初始化
    init_bcache(); // IO缓冲区初始化
    init_icache(); // inode表初始化
    init_disk();   // 硬盘初始化
    init_proc1();  // 启动1号用户任务
    __sync_synchronize();
    cpu_ok = true;
  } else {
    while (! cpu_ok)
      ;
    init_page();
    init_trap();
    init_plic();
    __sync_synchronize();
  }
  sti();
  task_schedule();
}
