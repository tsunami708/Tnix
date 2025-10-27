#pragma once
#define NULL nullptr

typedef signed char i8;
typedef short i16;
typedef int i32;
typedef long i64;

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long u64;

typedef u64 pte_t;
typedef pte_t* pagetable_t;
typedef u32 dev_t;

#define align_down(x, align) ((x) & ~((align) - 1))
#define align_up(x, align)   (((x) + (align) - 1) & ~((align) - 1))
#define min(a, b)            ((a) < (b) ? (a) : (b))
