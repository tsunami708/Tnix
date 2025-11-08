#pragma once
#define NULL nullptr
int fork(void);
int exit(int status);
int exec(const char* path, char* arg[]);
int wait(int* status);
int read(int fd, void* buf, int count);
int write(int fd, const void* buf, int count);
int open(const char* path, int mode);
int dup(int fd);
int close(int fd);
int link(const char* oldpath, const char* newpath);
int unlink(const char* path);
int mkdir(const char* path);
int rmdir(const char* path);
int mknod(const char* path, unsigned int dev); // 只能创建字符设备文件
int chdir(const char* path);
int rename(const char* oldpath, const char* newpath);
void* mmap(void* addr, unsigned int length, int prot);
int munmap(void* addr, unsigned int length);

enum mode {
  O_RDONLY = 0b1,
  O_WRONLY = 0b10,
  O_RDWR = 0b100,
  O_TRUNC = 0b1000,
  O_APPEND = 0b10000,
  O_CREAT = 0b100000,
};