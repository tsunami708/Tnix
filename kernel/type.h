#pragma once
#define NULL (void*)0

typedef int bool;
#define true  1
#define false 0

typedef signed char i8;
typedef short i16;
typedef int i32;
typedef long long i64;

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

typedef u64 pte_t;
typedef pte_t* pagetable_t;


#define align_down(x, align) ((x) & ~((align) - 1))
#define align_up(x, align)   (((x) + (align) - 1) & ~((align) - 1))
#define min(a, b)            ((a) < (b) ? (a) : (b))
