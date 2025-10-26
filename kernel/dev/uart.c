#include "config.h"

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


static inline char
r_reg(int r)
{
  return *reg(r);
}

static inline void
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
void
uart_put_syn(char c)
{
  while ((r_reg(LSR) & LSR_W) == 0)
    ;
  w_reg(THR, c);
}

void
do_uart_irq(void)
{
  char ch = uart_get();
  if (ch != (char)-1) {
    if (ch == '\r')
      uart_put_syn('\n');
    else if (ch == 127) { // backspace
      uart_put_syn('\b');
      uart_put_syn(' ');
      uart_put_syn('\b');
    } else if (ch == 16)
      uart_put_syn('P'); // ctrl+p
    else
      uart_put_syn(ch);
  }
}