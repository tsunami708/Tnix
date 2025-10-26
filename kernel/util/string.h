#pragma once
#include "util/types.h"

void* memset1(void* dst, int c, u32 n);
void* memcpy1(void* dst, const void* src, u32 n);
int strncmp1(const char* p, const char* q, u32 n);
char* strncpy1(char* s, const char* t, int n);
int strlen1(const char* s);
