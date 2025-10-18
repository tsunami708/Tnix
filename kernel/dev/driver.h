#pragma once

char uart_get();
void uart_put_syn(char c);


struct buf;
void disk_read(struct buf* buf);
void disk_write(struct buf* buf);