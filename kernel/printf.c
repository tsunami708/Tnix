#include "printf.h"
#include "spinlock.h"
/*仅支持d,u,s,x且数值均视为64位*/

struct var_para {
  u64 a1;
  u64 a2;
  u64 a3;
  u64 a4;
  u64 a5;
  u8  i;
};

INIT_SPINLOCK(pr);
volatile bool panicing = false;

static const char digits[] = "0123456789ABCDEF";

#define UART_TX 0x10000000
static inline void
uart_putc(const char c)
{
  *(volatile unsigned char*)UART_TX = c;
}


#define parse_para(vp)                                                                                                 \
  asm volatile("sd a1, %0 " : "=m"(vp.a1));                                                                            \
  asm volatile("sd a2, %0 " : "=m"(vp.a2));                                                                            \
  asm volatile("sd a3, %0 " : "=m"(vp.a3));                                                                            \
  asm volatile("sd a4, %0 " : "=m"(vp.a4));                                                                            \
  asm volatile("sd a5, %0 " : "=m"(vp.a5));                                                                            \
  vp.i = 0

#define fetch_para(vp) (*(u64*)((u64)vp + 8 * vp->i++))

static void
printstr(const char* ch)
{
  while (*ch != '\0') {
    uart_putc(*ch);
    ++ch;
  }
}
static void
printuint(i8 ns, struct var_para* vp) // ns表示进制
{
  int  i       = 0;
  u64  num     = fetch_para(vp);
  char buf[21] = { 0 };
  if (num == 0) {
    uart_putc('0');
    return;
  }
  while (num > 0) {
    buf[i++] = digits[num % ns];
    num /= ns;
  }
  while (i--)
    uart_putc(buf[i]);
}

static void
printint(i8 ns, struct var_para* vp)
{
  int i   = 0;
  i64 num = fetch_para(vp);
  if (num < 0)
    uart_putc('-');
  char buf[21] = { 0 };
  if (num == 0) {
    uart_putc('0');
    return;
  }
  num = num < 0 ? -num : num;
  while (num > 0) {
    buf[i++] = digits[num % ns];
    num /= ns;
  }
  while (i--)
    uart_putc(buf[i]);
}

void
print(const char* fmt, ...)
{
  struct var_para vp;
  parse_para(vp);
  acquire_spin(&pr);
  while (!panicing && *fmt != '\0') {
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
    uart_putc(*fmt);
    ++fmt;
  }
  release_spin(&pr);
  if (panicing)
    goto bad;
  return;
bad:
  while (1)
    ;
}

void
panic(const char* fmt, ...)
{
  if (__atomic_load_n(&panicing, __ATOMIC_SEQ_CST))
    goto bad;
  __atomic_store_n(&panicing, true, __ATOMIC_SEQ_CST);
  struct var_para vp;
  parse_para(vp);
  printstr("\n\n\npanic: ");
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
    uart_putc(*fmt);
    ++fmt;
  }
bad:
  while (1)
    ;
}