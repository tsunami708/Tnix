#pragma once
// 页帧大小
#define PGSIZE 4096

// 硬件属性
#define NCPU       4
#define PHY_MEMORY 0x80000000UL
#define PHY_SIZE   0x1FA000000UL // 8GB
#define PHY_TOP    PHY_MEMORY + PHY_SIZE
#define NPAGE      PHY_SIZE / PGSIZE