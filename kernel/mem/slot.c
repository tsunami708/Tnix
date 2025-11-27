#include "config.h"
#include "mem/alloc.h"
#include "util/spinlock.h"
#include "util/printf.h"
#include "task/task.h"
#include "fs/file.h"
#include "fs/inode.h"

#define slot_define(name, num, type)                                                                                   \
  struct name {                                                                                                        \
    struct spinlock lock;                                                                                              \
    u32 refc;                                                                                                          \
    struct chunk_##name {                                                                                              \
      type obj;                                                                                                        \
      struct name* belong;                                                                                             \
      bool inuse;                                                                                                      \
    }* slot;                                                                                                           \
    struct name* next;                                                                                                 \
  } name[num] = { [0 ...(num - 1)] = { .lock.lname = #name } };                                                        \
  type* alloc_##name(void)                                                                                             \
  {                                                                                                                    \
    struct name* cur = name;                                                                                           \
    while (cur) {                                                                                                      \
      spin_get(&cur->lock);                                                                                            \
      if (cur->refc == 0)                                                                                              \
        cur->slot = (struct chunk_##name*)alloc_page()->paddr;                                                         \
      if (cur->refc == PGSIZE / sizeof(struct chunk_##name)) {                                                         \
        spin_put(&cur->lock);                                                                                          \
        cur = cur->next;                                                                                               \
        continue;                                                                                                      \
      }                                                                                                                \
      for (int i = 0; i < PGSIZE / sizeof(struct chunk_##name); ++i) {                                                 \
        if (cur->slot[i].inuse == false) {                                                                             \
          ++cur->refc;                                                                                                 \
          cur->slot[i].inuse = true;                                                                                   \
          cur->slot[i].belong = cur;                                                                                   \
          spin_put(&cur->lock);                                                                                        \
          return &cur->slot[i].obj;                                                                                    \
        }                                                                                                              \
      }                                                                                                                \
      spin_put(&cur->lock);                                                                                            \
    }                                                                                                                  \
    panic("%s: slot exhausted", __func__);                                                                             \
  }                                                                                                                    \
  void free_##name(type* obj)                                                                                          \
  {                                                                                                                    \
    struct name* cur = ((struct chunk_##name*)obj)->belong;                                                            \
    spin_get(&cur->lock);                                                                                              \
    ((struct chunk_##name*)obj)->inuse = false;                                                                        \
    --cur->refc;                                                                                                       \
    if (cur->refc == 0) {                                                                                              \
      free_page(page((u64)cur->slot));                                                                                 \
      cur->slot = NULL;                                                                                                \
    }                                                                                                                  \
    spin_put(&cur->lock);                                                                                              \
  }                                                                                                                    \
  static void init_##name(void)                                                                                        \
  {                                                                                                                    \
    for (int i = 0; i < num - 1; ++i)                                                                                  \
      name[i].next = &name[i + 1];                                                                                     \
    name[num - 1].next = NULL;                                                                                         \
  }

slot_define(vma_slot, NVMA_SLOT, struct vma);
slot_define(mm_struct_slot, NMM_STURCT_SLOT, struct mm_struct);
slot_define(fs_struct_slot, NFS_STRUCT_SLOT, struct fs_struct);
slot_define(file_slot, NFILE_SLOT, struct file);

void
init_slot(void)
{
  init_vma_slot();
  init_mm_struct_slot();
  init_fs_struct_slot();
  init_file_slot();
}