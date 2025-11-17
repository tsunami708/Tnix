#pragma once
#include "types.h"

void* memset(void* dst, int c, u32 n);
void* memcpy(void* dst, const void* src, u32 n);
char* strncpy(char* s, const char* t, int n);
int strlen(const char* s);
