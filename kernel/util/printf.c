#include "util/printf.h"
#include "util/spinlock.h"

INIT_SPINLOCK(pr); // 防止多核交错打印

static const char digits[] = "0123456789ABCDEF";

extern void uart_put_syn(char c);

#define parse_para(vp)                                                                                                 \
  asm volatile("sd a1, %0 " : "=m"(vp.a1));                                                                            \
  asm volatile("sd a2, %0 " : "=m"(vp.a2));                                                                            \
  asm volatile("sd a3, %0 " : "=m"(vp.a3));                                                                            \
  asm volatile("sd a4, %0 " : "=m"(vp.a4));                                                                            \
  asm volatile("sd a5, %0 " : "=m"(vp.a5));                                                                            \
  asm volatile("sd a6, %0 " : "=m"(vp.a6));                                                                            \
  asm volatile("sd a7, %0 " : "=m"(vp.a7));                                                                            \
  vp.i = 0

#define fetch_para(vp) (*(u64*)((u64)vp + 8 * vp->i++))

/*仅支持d,u,s,x且数值均视为64位*/
/*
RISC-V当参数<=8时从左到右a0~a7寄存器传递
*/
struct var_para {
  u64 a1;
  u64 a2;
  u64 a3;
  u64 a4;
  u64 a5;
  u64 a6;
  u64 a7;
  u8 i;
};


static void
printstr(const char* ch)
{
  while (*ch != '\0') {
    uart_put_syn(*ch);
    ++ch;
  }
}
static void
printuint(i8 ns, struct var_para* vp) // ns表示进制
{
  int i = 0;
  u64 num = fetch_para(vp);
  char buf[21] = { 0 };
  if (num == 0) {
    uart_put_syn('0');
    return;
  }
  while (num > 0) {
    buf[i++] = digits[num % ns];
    num /= ns;
  }
  while (i--)
    uart_put_syn(buf[i]);
}

static void
printint(i8 ns, struct var_para* vp)
{
  int i = 0;
  long num = fetch_para(vp);
  if (num < 0)
    uart_put_syn('-');
  char buf[21] = { 0 };
  if (num == 0) {
    uart_put_syn('0');
    return;
  }
  num = num < 0 ? -num : num;
  while (num > 0) {
    buf[i++] = digits[num % ns];
    num /= ns;
  }
  while (i--)
    uart_put_syn(buf[i]);
}

void
print(const char* fmt, ...)
{
  struct var_para vp;
  parse_para(vp);
  spin_get(&pr);
  while (*fmt != '\0') {
    if (*fmt == '%') {
      ++fmt;
      if (*fmt == 'd' || *fmt == 'u' || *fmt == 'x' || *fmt == 's') {
        switch (*fmt) {
        case 'd':
          printint(10, &vp);
          break;
        case 'u':
          printuint(10, &vp);
          break;
        case 'x':
          printstr("0x");
          printuint(16, &vp);
          break;
        case 's':
          printstr((const char*)fetch_para((&vp)));
          break;
        }
        ++fmt;
        continue;
      }
      --fmt;
    }
    uart_put_syn(*fmt);
    ++fmt;
  }
  spin_put(&pr);
}

void
panic(const char* fmt, ...)
{
  struct var_para vp;
  parse_para(vp);
  spin_get(&pr); // 故意不释放锁
  printstr("\n\npanic: ");
  while (*fmt != '\0') {
    if (*fmt == '%') {
      ++fmt;
      if (*fmt == 'd' || *fmt == 'u' || *fmt == 'x' || *fmt == 's') {
        switch (*fmt) {
        case 'd':
          printint(10, &vp);
          break;
        case 'u':
          printuint(10, &vp);
          break;
        case 'x':
          printstr("0x");
          printuint(16, &vp);
          break;
        case 's':
          printstr((const char*)fetch_para((&vp)));
          break;
        }
        ++fmt;
        continue;
      }
      --fmt;
    }
    uart_put_syn(*fmt);
    ++fmt;
  }
  uart_put_syn('\n');
  while (1)
    ;
}