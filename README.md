# Tnix
基于RISC-V平台的64位多核操作系统
## 功能
  - ls
  - cat
  - echo
  - rm
  - mkdir
  - rmdir
  - touch
  - shutdown
  - vi

*命令程序是极简版本,和Linux实际命令相差较大*

## 开发与运行环境

*开发工具*
- git 2.51.1
- cmake 3.31.6
- make 4.4.1
- gcc-riscv64-linux-gnu
- gcc(g++) 15.2.1
- gdb 16.3
- qemu 10.1.2
- python 3.14.0

1. **Fedora-43**(x86_64)

```bash
sudo dnf update -y
sudo dnf install man clear git make cmake -y
sudo dnf install gcc g++ gdb -y
sudo dnf install gcc-riscv64-linux-gnu -y
sudo dnf install qemu-system-riscv64 -y
```
2. **Docker**
```bash
docker pull tsunami849/tnix:latest
docker run -it --name Tnix tsunami849/tnix:latest
# docker start -ai Tnix 进入已经创建的容器
```
## 项目构建
```bash
cmake -B build
cd build
make

#直接运行
make qemu 

#调试
make gdb 
gdb
```

## 参考资料
1. [Operating-System-Concepts](https://os.ecci.ucr.ac.cr/slides/Abraham-Silberschatz-Operating-System-Concepts-10th-2018.pdf)
2. [A simple, Unix-like teaching operating system](https://pdos.csail.mit.edu/6.828/2025/xv6/book-riscv-rev5.pdf)
3. [elf-64-gen](https://github.com/yifengyou/parser-elf/blob/master/misc/elf-64-gen.pdf)
4. RISC-V体系结构编程与实践
5. [riscv-plic-spec](https://github.com/riscv/riscv-plic-spec/blob/master/riscv-plic-1.0.0.pdf)
6. [riscv-privileged](https://mirror.iscas.ac.cn/riscv-toolchains/release/riscv/riscv-isa-manual/Release%20riscv-isa-release-5953d84-2025-05-27/riscv-privileged.pdf)
7. [gnu-manuals](https://www.gnu.org/manual/manual.html)