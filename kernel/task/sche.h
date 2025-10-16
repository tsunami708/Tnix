#pragma once

struct spinlock;

void sleep(void* chan, struct spinlock* lock);

void wakeup(void* chan);