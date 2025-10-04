#pragma once
// 最多支持6个参数,超过是未定义行为
void print(const char* fmt, ...);

void panic(const char* fmt, ...) __attribute__((noreturn));