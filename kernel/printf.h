#pragma once
// 最多支持8个参数(7个不定参),超过是未定义行为
void print(const char* fmt, ...);

// 最多支持8个参数(7个不定参),超过是未定义行为
void panic(const char* fmt, ...) __attribute__((noreturn));