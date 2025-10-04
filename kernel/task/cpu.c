#include "cpu.h"
#include "config.h"

static struct cpu cpus[NCPU];

struct cpu*
mycpu()
{
  return cpus + cpuid();
}