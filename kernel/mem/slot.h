#pragma once

#define slot_declare(name, type)                                                                                       \
  type* alloc_##name(void);                                                                                            \
  void free_##name(type* obj);

slot_declare(vma_slot, struct vma);
slot_declare(mm_struct_slot, struct mm_struct);
slot_declare(fs_struct_slot, struct fs_struct);