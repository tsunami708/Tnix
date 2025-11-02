#pragma once
#define NULL nullptr
int fork(void);
int exit(int status);
int exec(const char* path, char* arg[]);
int wait(int* status);