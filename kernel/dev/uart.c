#include "config.h"
#include "types.h"
#include "util/spinlock.h"

#define RHR 0
#define THR 0
#define IER 1
#define FCR 2
#define ISR 2
#define LCR 3
#define MCR 4
#define LSR 5
#define MSR 6

#define LSB 0
#define MSB 1

#define reg(r) (volatile char*)(UART0 + r)


static inline __attribute__((always_inline)) char
r_reg(int r)
{
  return *reg(r);
}

static inline __attribute__((always_inline)) void
w_reg(int r, char v)
{
  *reg(r) = v;
}

void
init_uart(void)
{
  // 关闭中断
  w_reg(IER, 0x00);
  // 设置波特率
  w_reg(LCR, 0x80);
  w_reg(LSB, 0x03);
  w_reg(MSB, 0x00);
  // 传输字长
  w_reg(LCR, 0x03);
  // 启动管道传输
  w_reg(FCR, 0x07);
  // 开启T/R中断
  w_reg(IER, 0x02);
}

#define LSR_R (1 << 0)
#define LSR_W (1 << 5)



char
uart_get(void)
{
  if (r_reg(LSR) & LSR_R)
    return r_reg(RHR);
  return -1;
}


struct spinlock;
static struct spinlock tx = { .lname = "tx" };
extern void sleep(void* chan, struct spinlock* lock);
extern void wakeup(void* chan);
extern void copy_from_user(void* kdst, const void* usrc, u32 bytes);
void
uart_put_syn(char c)
{
  spin_get(&tx);
  while ((r_reg(LSR) & LSR_W) == 0)
    ;
  w_reg(THR, c);
  spin_put(&tx);
}

void
uart_write(const char* ustr, u32 len)
{
  static char buf[32] = { 0 };
  spin_get(&tx);
  while (len) {
    u32 size = min(len, sizeof(buf));
    copy_from_user(buf, ustr, size);
    for (int i = 0; i < size; ++i) {
      while ((r_reg(LSR) & LSR_W) == 0)
        sleep((void*)UART0, &tx);
      w_reg(THR, buf[i]);
    }
    len -= size;
    ustr += size;
  }
  spin_put(&tx);
}

void
do_uart_irq(void)
{
  if (r_reg(LSR) & LSR_W)
    wakeup((void*)UART0);

  extern void do_console_irq(char);
  char ch = uart_get();
  if (ch != (char)-1)
    do_console_irq(ch);
}