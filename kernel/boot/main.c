#include "cpu.h"

volatile bool cpu_ok = false;

extern void init_memory();
extern void init_page();
extern void init_trap();
extern void init_plic();
extern void init_systemd();
extern void init_uart();
extern void init_iobuf();
extern void init_disk();
extern void task_schedule();


void
main()
{
  if (cpuid() == 0) {
    init_uart();    // 终端初始化
    init_memory();  // 物理地址初始化
    init_page();    // 内核页表初始化
    init_trap();    // 陷阱处理初始化
    init_plic();    // 中断控制器初始化
    init_iobuf();   // IO缓冲区初始化
    init_disk();    // 硬盘初始化
    init_systemd(); // 启动1号用户任务
    __sync_synchronize();
    cpu_ok = true;
  } else {
    while (!cpu_ok)
      ;
    init_page();
    init_trap();
    init_plic();
    __sync_synchronize();
  }
  sti();
  task_schedule();
}
