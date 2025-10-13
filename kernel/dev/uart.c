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

#define reg(r) (volatile uchar*)(UART0 + r)

typedef unsigned char uchar;

static inline uchar
r_reg(int r)
{
  return *reg(r);
}

static inline void
w_reg(int r, uchar v)
{
  *reg(r) = v;
}

void
init_uart()
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

uchar
uart_get()
{
again:
  if (r_reg(LSR) & LSR_R)
    return r_reg(RHR);
  goto again;
  // return -1;
}
void
uart_put(uchar c)
{
  while ((r_reg(LSR) & LSR_W) == 0)
    ;
  w_reg(THR, c);
}

void
do_uart_irq()
{
  // TEST
  init_uart();
  uart_put('H');
  uart_put('e');
  uart_put('l');
  uart_put('l');
  uart_put('o');
  uart_put(',');
  uart_put('w');
  uart_put('o');
  uart_put('r');
  uart_put('l');
  uart_put('d');
  uart_put('\n');
  while (1) {
    uchar c = uart_get();
    if (c == '\r')
      uart_put('\n');
    else if (c == 0x8) {
      uart_put('\b');
      uart_put(' ');
      uart_put('\b');
    } else
      uart_put(c);
  }
}