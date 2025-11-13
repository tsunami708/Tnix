#pragma once
#define USER
#include "kernel/fs/file.h"
#include "kernel/errno.h"
#define NULL nullptr
int fork(void);
int exit(int status);
int exec(const char* path, char* arg[]);
int wait(int* status);
int read(int fd, void* buf, int count);
int write(int fd, const void* buf, int count);
int lseek(int fd, int off);
int open(const char* path, enum mode mode);
int dup(int fd);
int close(int fd);
int link(const char* oldpath, const char* newpath);
int unlink(const char* path);
int mkdir(const char* path);
int rmdir(const char* path);
int mknod(const char* path, unsigned int dev); // 只能创建字符设备文件
int chdir(const char* path);
void* mmap(void* addr, unsigned int length, int prot);
int munmap(void* addr, unsigned int length);
