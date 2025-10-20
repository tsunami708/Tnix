#pragma once

char uart_get();
void uart_put_syn(char c);


struct iobuf;
void disk_read(struct iobuf* buf);
void disk_write(struct iobuf* buf);