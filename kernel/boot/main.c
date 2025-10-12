#include "cpu.h"

volatile bool cpu_ok = false;

extern void init_mem();
extern void init_page();
extern void init_trap();
extern void init_systemd();
extern void task_schedule();

void
main()
{
  if (cpuid() == 0) {
    init_mem();     // 物理地址初始化
    init_page();    // 内核页表初始化
    init_trap();    // 陷阱处理初始化
    init_systemd(); // 启动1号用户任务
    __sync_synchronize();
    cpu_ok = true;
  } else {
    while (!cpu_ok)
      ;
    init_page();
    init_trap();
    __sync_synchronize();
  }

  sti();
  task_schedule();
}