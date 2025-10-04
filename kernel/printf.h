#pragma once

void panic(const char* fmt, ...);
// 最多支持6个参数,超过是未定义行为
void printf(const char* fmt, ...);