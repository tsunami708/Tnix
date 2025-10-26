#include "fs/file.h"
#include "util/spinlock.h"
#include "util/printf.h"

#define MFILES 50
static struct {
  struct spinlock lock;
  struct file files[MFILES];
} file_fcache = { .lock.lname = "file_fcache" };

struct file*
alloc_file(void)
{
  acquire_spin(&file_fcache.lock);
  for (int i = 0; i < MFILES; ++i) {
    if (file_fcache.files[i].refc == 0) {
      file_fcache.files[i].refc = 1;
      file_fcache.files[i].off = 0;
      release_spin(&file_fcache.lock);
    }
  }
  panic("file exhausted");
}

void
dup_file(struct file* f)
{
  acquire_spin(&file_fcache.lock);
  if (f->refc < 1)
    panic("dup_file");
  ++f->refc;
  release_spin(&file_fcache.lock);
}

void
close_file(struct file* f)
{
  acquire_spin(&file_fcache.lock);
  if (f->refc == 0)
    panic("close_file");
  --f->refc;
  release_spin(&file_fcache.lock);
}
