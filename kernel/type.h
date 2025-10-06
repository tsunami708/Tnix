#pragma once
#define bool  int
#define true  1
#define false 0
#define NULL  0

#define i8  signed char
#define i16 short
#define i32 int
#define i64 long long

#define u8  unsigned char
#define u16 unsigned short
#define u32 unsigned int
#define u64 unsigned long long

#define align_down(x, align) ((x) & ~((align) - 1))
#define align_up(x, align)   (((x) + (align) - 1) & ~((align) - 1))

#define min(a, b) (a < b ? a : b)

#define pte_t       u64
#define pagetable_t pte_t* // 指示根页表