#构建工具
QEMU = qemu-system-riscv64
CC = riscv64-linux-gnu-gcc
AS = riscv64-linux-gnu-as
LD = riscv64-linux-gnu-ld
OBJCOPY = riscv64-linux-gnu-objcopy
OBJDUMP = riscv64-linux-gnu-objdump

#编译器属性
CFLAGS = -Wall -Werror -Wno-unknown-attributes -Wstrict-prototypes -O0 -fno-omit-frame-pointer -ggdb -gdwarf-2
CFLAGS += -MD -mcmodel=medany -std=gnu23
CFLAGS += -fno-common -nostdlib -ffreestanding
CFLAGS += -fno-builtin-strncpy -fno-builtin-strncmp -fno-builtin-strlen -fno-builtin-memset
CFLAGS += -fno-builtin-memmove -fno-builtin-memcmp -fno-builtin-log -fno-builtin-bzero
CFLAGS += -fno-builtin-strchr -fno-builtin-exit -fno-builtin-malloc -fno-builtin-putc
CFLAGS += -fno-builtin-free
CFLAGS += -fno-builtin-memcpy -Wno-main
CFLAGS += -fno-builtin-printf -fno-builtin-fprintf -fno-builtin-vprintf
CFLAGS += $(shell $(CC) -fno-stack-protector -E -x c /dev/null >/dev/null 2>&1 && echo -fno-stack-protector)
CFLAGS += -fno-pie -no-pie 
CFLAGS += -Ikernel -I.


#链接器属性
LDFLAGS = -z max-page-size=4096

#模拟器属性
ifndef CPUS
CPUS := 4
endif
QEMUOPTS = -bios none -kernel $K/kernel -m 512M -smp $(CPUS) -nographic -cpu rv64,d=false,f=false,zfa=false
QEMUOPTS += -global virtio-mmio.force-legacy=false
QEMUOPTS += -drive file=fs.img,if=none,format=raw,id=x0
QEMUOPTS += -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0

#构建目标文件
K=kernel
OBJS = \
  $K/boot/entry.o \
	$K/boot/start.o \
	$K/boot/main.o \
	$K/mem/alloc.o \
	$K/mem/vm.o \
	$K/task/sche.o \
	$K/task/switch.o \
	$K/task/systemd.o \
	$K/dev/uart.o \
	$K/dev/disk.o \
	$K/fs/bio.o \
	$K/fs/fs.o \
	$K/fs/inode.o \
	$K/fs/dir.o \
	$K/fs/file.o \
	$K/util/spinlock.o \
	$K/util/sleeplock.o \
	$K/util/printf.o \
	$K/util/list.o \
	$K/util/string.o \
	$K/trap/trap.o \
	$K/trap/pt_reg.o \
	$K/trap/plic.o \
	$K/syscall/syscall.o \
	$K/syscall/usys.o \

U=user

$K/kernel: $(OBJS) $K/kernel.ld
	$(LD) $(LDFLAGS) -T $K/kernel.ld -o $K/kernel $(OBJS) 
	$(OBJDUMP) -S $K/kernel > $K/kernel.asm
	$(OBJDUMP) -t $K/kernel | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $K/kernel.sym

$K/%.o: $K/%.S
	$(CC) $(CFLAGS) -g -c -o $@ $<


-include kernel/*.d user/*.d

clean:
	find . -type f \( \
	-name "*.tex" -o -name "*.dvi" -o -name "*.idx" -o -name "*.aux" -o \
	-name "*.log" -o -name "*.ind" -o -name "*.ilg" -o \
	-name "*.o" -o -name "*.d" -o -name "*.asm" -o -name "*.sym" \
	\) -exec rm -f {} +
	rm -f $K/kernel *.dtb mkfs/mkfs fs.img

fs.img:
	g++ mkfs/mkfs.cc -o mkfs/mkfs -I. -std=c++17 -g
	mkfs/mkfs user

qemu: $K/kernel fs.img
	$(QEMU) -machine virt $(QEMUOPTS)

gdb: $K/kernel fs.img
	$(QEMU) -machine virt $(QEMUOPTS) -S -s

dts: $K/kernel
	$(QEMU) -machine virt,dumpdtb=device.dtb $(QEMUOPTS)
	dtc -I dtb -O dts -o device.dts device.dtb
