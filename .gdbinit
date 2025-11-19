define ls
  layout src
end

define la
  layout asm
end

define slock
  set scheduler-locking on
end

define sunlock
  set scheduler-locking off
end


define pb
  set $bcache_ptr = (struct buf*)&bcache.bufs
  set $count = 0
  set $i = 0
  while $i < 16
    set $buf = &$bcache_ptr[$i]
    if ($buf->refc != 0)
      printf "  buf: %p, refc = %d\n", $buf, $buf->refc
      set $count = $count + 1
    end
    set $i = $i + 1
  end
end

define pf
  set $fcache_ptr = (struct file*)&fcache.files
  set $count = 0
  set $i = 0
  while $i < 16
    set $file = &$fcache_ptr[$i]
    if ($file->refc != 0)
      printf "  file: %p, refc = %d\n", $file, $file->refc
      set $count = $count + 1
    end
    set $i = $i + 1
  end
end

define pi
  set $icache_ptr = (struct inode*)&icache.inodes
  set $count = 0
  set $i = 0
  while $i < 50
    set $inode = &$icache_ptr[$i]
    if ($inode->refc != 0)
      printf "  inode: %p, refc = %d\n", $inode, $inode->refc
      set $count = $count + 1
    end
    set $i = $i + 1
  end
end


set confirm off
set architecture riscv:rv64
target remote 127.0.0.1:1234
symbol-file build/kernel
set disassemble-next-line auto
set riscv use-compressed-breakpoints yes
