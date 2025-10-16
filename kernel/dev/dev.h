#pragma once

void init_uart();
void init_disk();

static inline void
init_device()
{
  init_uart();
  init_disk();
}