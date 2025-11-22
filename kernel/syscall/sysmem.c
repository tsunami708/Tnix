#include "errno.h"
#include "syscall/syscall.h"
#include "trap/pt_reg.h"
#include "mem/alloc.h"

u64
sys_alloc(struct pt_regs*)
{
  struct task* t = mytask();
  if (t->next_heap == USTACK)
    return 0;
  struct page* p = alloc_page_for_task(t);
  u16 attr = PTE_U | PTE_W | PTE_R;
  if (! task_vmmap(t, t->next_heap, p->paddr, PGSIZE, attr, HEAP)) {
    free_page_for_task(p);
    return 0;
  }
  u64 r = t->next_heap;
  t->next_heap += PGSIZE;
  return r;
}
u64
sys_free(struct pt_regs* pt)
{
  if (pt->a0 == 0)
    return -EINVAL;
  if (pt->a0 % PGSIZE)
    return -EINVAL;
  struct task* t = mytask();
  for (int i = 0; i < t->vmas.n; ++i) {
    if (t->vmas.v[i].type == HEAP && t->vmas.v[i].va == pt->a0) {
      vmunmap(t->pagetable, t->vmas.v[i].va, t->vmas.v[i].len);
      free_page_for_task(page(t->vmas.v[i].pa));
      for (int j = i; j < t->vmas.n - 1; j++)
        t->vmas.v[j] = t->vmas.v[j + 1];
      t->vmas.n--;
      return 0;
    }
  }
  return -EINVAL;
}