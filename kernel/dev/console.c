#include "util/types.h"
#include "util/spinlock.h"
#include "util/string.h"

extern void uart_put_syn(char);

#define CONSOLE_BUFFER_SIZE 128
static struct {
  struct spinlock lock;
  u32 r, w, e;
  char buf[CONSOLE_BUFFER_SIZE];
  char tb[8]; // \033 转义缓冲区
  bool tflag; // 转移标识
  u8 i;
} con = { .lock.lname = "console" };

static void
reset_tb(void)
{
  *(long*)con.tb = 0;
  con.tflag = false;
  con.i = 0;
}

static void
clean_console(void)
{
  reset_tb();
  con.r = con.w = con.e = 0;
  memset(con.buf, 0, CONSOLE_BUFFER_SIZE);
  uart_put_syn('\033');
  uart_put_syn('c');
}

// 丢弃转义字符,防止乱码
static void
try_transfer(void)
{
  if (con.tb[con.i - 1] == 'O') // f1 f2 f3 f4
    return;
  if (con.tb[con.i - 1] == '~' || con.tb[con.i - 1] == '\007' || con.tb[con.i - 1] == '\\')
    reset_tb();
  else if ((con.tb[con.i - 1] >= 'a' && con.tb[con.i - 1] <= 'z')
           || (con.tb[con.i - 1] >= 'A' && con.tb[con.i - 1] <= 'Z'))
    reset_tb();
}

static void
console_putc(char ch)
{
  acquire_spin(&con.lock);
  if (!con.tflag) {
    if (ch == '\b') {
      if (con.buf[con.e - 1] != '\n') {
        --con.e;
        --con.w;
        uart_put_syn(ch);
      }
    } else if (ch == '\033') {
      con.tflag = true;
      con.tb[con.i++] = '\033';
    } else {
      ++con.w;
      con.buf[con.e++] = ch;
      uart_put_syn(ch);
    }
  } else {
    con.tb[con.i++] = ch;
    try_transfer();
  }
  release_spin(&con.lock);
}

#define BACKSPACE 127
#define CTRL(x)   (x - '@')
void
do_console_irq(char ch)
{
  switch (ch) {
  case '\033':
    console_putc('\033');
    break;
  case BACKSPACE:
    console_putc('\b');
    console_putc(' ');
    console_putc('\b');
    break;
  case '\r':
  case '\n':
    console_putc('\n');
    break;
  case 32 ... 126: // 可显示字符
    console_putc(ch);
    break;
  case CTRL('N'): // 清屏
    clean_console();
    break;
  }
}
