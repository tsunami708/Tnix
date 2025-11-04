#pragma once

struct spinlock;

void yield(void);

void sleep(void* chan, struct spinlock* lock);

void wakeup(void* chan);

void kill(void);