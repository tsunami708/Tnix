#include "config.h"
#include "types.h"
#include "util/spinlock.h"
#include "util/string.h"
#include "util/printf.h"
#include "task/sche.h"
#include "fs/file.h"


extern void uart_put_syn(char);
extern void copy_to_user(void* udst, const void* ksrc, u32 bytes);
extern void copy_from_user(void* kdst, const void* usrc, u32 bytes);

#define CONSOLE_BUFFER_SIZE 128
static struct {
  struct spinlock lock;
  int r, w;
  char buf[CONSOLE_BUFFER_SIZE];
  char tb[8]; // \033序列转义缓冲区
  bool tflag; // 转义标识
  u8 i;
} con = { .lock.lname = "console" };
#define console_readable_len()  ((con.w - con.r + CONSOLE_BUFFER_SIZE) % CONSOLE_BUFFER_SIZE)
#define console_writeable_len() ((con.r - con.w + CONSOLE_BUFFER_SIZE) % CONSOLE_BUFFER_SIZE)
#define console_isfull()        ((con.w + 1) % CONSOLE_BUFFER_SIZE == con.r)
#define console_isempty()       (con.w == con.r)
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
  con.r = con.w = 0;
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

// 键盘输入回显
static void
console_putc(char ch)
{
  spin_get(&con.lock);
  if (! con.tflag) {
    if (ch == '\b') {
      if (con.buf[con.w - 1] != '\n' && con.w != con.r) {
        con.w = (con.w - 1 + CONSOLE_BUFFER_SIZE) % CONSOLE_BUFFER_SIZE;
        uart_put_syn(ch);
      }
    } else if (ch == '\033') {
      con.tflag = true;
      con.tb[con.i++] = '\033';
    } else {
      if (! console_isfull()) {
        con.buf[con.w] = ch;
        con.w = (con.w + 1) % CONSOLE_BUFFER_SIZE;
        uart_put_syn(ch);
      } else
        print("console buffer is full!\n");
    }
  } else {
    con.tb[con.i++] = ch;
    try_transfer();
  }
  spin_put(&con.lock);
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
    wakeup(&con.r);
    break;
  case 32 ... 126: // 可显示字符
    console_putc(ch);
    break;
  case CTRL('N'): // 清屏
    clean_console();
    uart_put_syn('$');
    uart_put_syn('>');
    break;
  case CTRL('P'):
    extern void dump_all_task();
    dump_all_task();
    break;
  case CTRL('D'):
    console_putc('\x04');
    wakeup(&con.r);
    break;
  }
}

u32
console_read(void* udst, u32 len)
{
  spin_get(&con.lock);
  while (console_isempty())
    sleep(&con.r, &con.lock);
  len = min(len, console_readable_len());
  if (con.r < con.w)
    copy_to_user(udst, con.buf + con.r, len);
  else {
    u32 lslice1 = CONSOLE_BUFFER_SIZE - con.r, lslice2 = len - lslice1;
    copy_to_user(udst, con.buf + con.r, lslice1);
    copy_to_user(udst + lslice1, con.buf, lslice2);
  }
  con.r = (con.r + len) % CONSOLE_BUFFER_SIZE;
  spin_put(&con.lock);
  return len;
}

u32
console_write(const void* usrc, u32 len)
{
  extern void uart_write(const char*, u32);
  uart_write(usrc, len);
  return len;
}

void
init_console(void)
{
  extern void init_uart(void);
  extern struct dev_op devsw[NDEV];
  init_uart();
  devsw[CONSOLE].read = console_read;
  devsw[CONSOLE].write = console_write;
  devsw[CONSOLE].valid = true;
}