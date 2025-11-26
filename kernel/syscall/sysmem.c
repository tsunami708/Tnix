#include "syscall/syscall.h"
#include "trap/pt_reg.h"
#include "mem/alloc.h"
#include "mem/slot.h"

long
sys_alloc(struct pt_regs*)
{
  struct task* t = mytask();
  if (t->mm_struct->next_heap == USTACK)
    return 0;
  struct page* p = alloc_page_for_task(t);
  u16 attr = PTE_U | PTE_W | PTE_R;
  task_vmmap(t, t->mm_struct->next_heap, p->paddr, PGSIZE, attr, HEAP);
  long r = t->mm_struct->next_heap;
  t->mm_struct->next_heap += PGSIZE;
  return r;
}
long
sys_free(struct pt_regs* pt)
{
  if (pt->a0 == 0)
    return -1;
  if (pt->a0 % PGSIZE)
    return -1;
  struct task* t = mytask();
  struct list_node* node = t->mm_struct->vma_head.next;
  while (node != &t->mm_struct->vma_head) {
    struct vma* vma = container_of(node, struct vma, node);
    node = node->next;
    if (vma->type == HEAP && vma->pa == pt->a0) {
      list_remove(&vma->node);
      free_vma_slot(vma);
    }
  }
  return -1;
}