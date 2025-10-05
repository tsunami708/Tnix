#pragma once
#include "type.h"

#define S_PAGE 0 // 4KB
#define M_PAGE 1 // 2MB
#define G_PAGE 2 // 1GB

void vmmap(u64 va, u64 pa, u64 size, u16 attr, i8 granularity);