#pragma once
#include "types.h"

char uart_get(void);
void uart_put_syn(char c);


struct buf;
void disk_read(struct buf* buf);
void disk_write(struct buf* buf);


u32 console_read(void* dst, u32 len);
u32 console_write(const void* src, u32 len);