cmake_minimum_required(VERSION 3.31)
project(Tnix)
enable_language(ASM)
set(QEMU qemu-system-riscv64)
set(LD riscv64-linux-gnu-ld)
set(OBJDUMP riscv64-linux-gnu-objdump)
set(CMAKE_C_COMPILER riscv64-linux-gnu-gcc)
set(CMAKE_ASM_COMPILER ${CMAKE_C_COMPILER})
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


set(CMAKE_C_FLAGS
  "-Wall -Werror -Wno-unknown-attributes -Wstrict-prototypes -Wno-main \
  -O0 -g -gdwarf-2 -nostdlib -std=gnu23\
  -fno-common -ffreestanding -fno-stack-protector -fomit-frame-pointer \
  -fno-pie -no-pie -mcmodel=medany \
  -fno-builtin-strncpy -fno-builtin-strncmp -fno-builtin-strlen \
  -fno-builtin-memset -fno-builtin-memmove -fno-builtin-memcmp \
  -fno-builtin-log -fno-builtin-bzero -fno-builtin-strchr \
  -fno-builtin-exit -fno-builtin-malloc -fno-builtin-putc \
  -fno-builtin-free -fno-builtin-memcpy -fno-builtin-printf \
  -fno-builtin-fprintf -fno-builtin-vprintf"
)
set(CMAKE_CXX_FLAGS "-g -std=gnu++17")
set(CMAKE_EXE_LINKER_FLAGS "-z max-page-size=4096")


if(NOT DEFINED CPUS)
  set(CPUS 4)
endif()
set(QEMUOPTS
  -bios none
  -kernel ${CMAKE_BINARY_DIR}/kernel
  -m 512M
  -smp ${CPUS}
  -nographic
  -cpu "rv64,d=false,f=false,zfa=false"
  -global "virtio-mmio.force-legacy=false"
  -drive "file=${CMAKE_BINARY_DIR}/fs.img,if=none,format=raw,id=x0"
  -device "virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0"
)

file(GLOB_RECURSE KERNEL_SOURCES
  "${CMAKE_SOURCE_DIR}/kernel/*.c"
  "${CMAKE_SOURCE_DIR}/kernel/*.S"
)
file(GLOB_RECURSE USER_SOURCES
  "${CMAKE_SOURCE_DIR}/user/src/*.c"
)

add_executable(kernel ${KERNEL_SOURCES})
target_include_directories(kernel PRIVATE ${CMAKE_SOURCE_DIR}/kernel)
set_target_properties(kernel PROPERTIES LINK_DEPENDS ${CMAKE_SOURCE_DIR}/kernel/kernel.ld)
target_link_options(kernel PRIVATE "LINKER:-T,${CMAKE_SOURCE_DIR}/kernel/kernel.ld")
add_custom_command(TARGET kernel POST_BUILD
  COMMAND ${OBJDUMP} -S $<TARGET_FILE:kernel> > ${CMAKE_BINARY_DIR}/kernel.asm
  COMMAND ${OBJDUMP} -t $<TARGET_FILE:kernel> | sed "1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d" > ${CMAKE_BINARY_DIR}/kernel.sym
  COMMENT "Compile kernel"
  VERBATIM
)

add_library(usys STATIC ${CMAKE_SOURCE_DIR}/user/src/usys.S)
target_include_directories(usys PUBLIC ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/user/include)
set(USER_PROGRAM_TARGETS)
foreach(source ${USER_SOURCES})
  get_filename_component(prog_name ${source} NAME_WE)
  list(APPEND USER_PROGRAM_TARGETS ${prog_name})
  add_executable(${prog_name} ${source})
  target_link_libraries(${prog_name} PRIVATE usys)
  target_link_options(${prog_name} PRIVATE "LINKER:-T,${CMAKE_SOURCE_DIR}/user/user.ld")
  set_target_properties(${prog_name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/user/bin)
endforeach()
add_custom_target(user DEPENDS ${USER_PROGRAM_TARGETS})


add_executable(mkfs ${CMAKE_SOURCE_DIR}/mkfs/mkfs.cc)
target_include_directories(mkfs PRIVATE ${CMAKE_SOURCE_DIR})

add_custom_target(fs
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/user/dev
  COMMAND ${CMAKE_BINARY_DIR}/mkfs ${CMAKE_SOURCE_DIR}/user
  DEPENDS mkfs user
  COMMENT "Build file system image"
)

add_custom_target(qemu
  COMMAND ${QEMU} -machine virt ${QEMUOPTS}
  DEPENDS kernel fs
  COMMENT "Running kernel in QEMU"
)

add_custom_target(gdb
  COMMAND ${QEMU} -machine virt -S -s ${QEMUOPTS}
  DEPENDS kernel fs
  COMMENT "Running kernel in QEMU-GDB"
)