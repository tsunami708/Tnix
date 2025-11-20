#!/usr/bin/env python3
import re
import sys
import os

def find_macro_values(file_paths):
  config = {}
  
  macro_pattern = re.compile(r'#define\s+(NFILE|NIOBUF|NINODE)\s+(\d+)')
  
  for file_path in file_paths:
    if not os.path.exists(file_path):
      raise FileNotFoundError(f"File {file_path} does not exist")
      
    try:
      with open(file_path, 'r') as f:
        for line in f:
          match = macro_pattern.search(line)
          if match:
            macro_name = match.group(1)
            macro_value = int(match.group(2))
            config[macro_name] = macro_value
            print(f"Found {macro_name} = {macro_value} in {file_path}")
    except Exception as e:
      raise Exception(f"Error reading {file_path}: {e}")
  
  missing_macros = []
  for macro in ['NFILE', 'NIOBUF', 'NINODE']:
    if macro not in config:
      missing_macros.append(macro)
  
  if missing_macros:
    raise ValueError(f"Missing macro definitions: {', '.join(missing_macros)}")
  
  return config

def generate_gdbinit(config):
  gdbinit_content = f'''define ls
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
  while $i < {config['NIOBUF']}
    set $buf = &$bcache_ptr[$i]
    if ($buf->refc != 0)
      printf "  buf: %p, refc = %d\\n", $buf, $buf->refc
      set $count = $count + 1
    end
    set $i = $i + 1
  end
end

define pf
  set $fcache_ptr = (struct file*)&fcache.files
  set $count = 0
  set $i = 0
  while $i < {config['NFILE']}
    set $file = &$fcache_ptr[$i]
    if ($file->refc != 0)
      printf "  file: %p, refc = %d\\n", $file, $file->refc
      set $count = $count + 1
    end
    set $i = $i + 1
  end
end

define pi
  set $icache_ptr = (struct inode*)&icache.inodes
  set $count = 0
  set $i = 0
  while $i < {config['NINODE']}
    set $inode = &$icache_ptr[$i]
    if ($inode->refc != 0)
      printf "  inode: %p, refc = %d\\n", $inode, $inode->refc
      set $count = $count + 1
    end
    set $i = $i + 1
  end
end

set confirm off
set architecture riscv:rv64
target remote 127.0.0.1:1234
symbol-file kernel
set disassemble-next-line auto
set riscv use-compressed-breakpoints yes
'''

  with open('.gdbinit', 'w') as f:
    f.write(gdbinit_content)

def main():
  if len(sys.argv) < 2:
    print("Usage: python generate_gdbinit.py <file1> [file2 ...]")
    sys.exit(1)
  
  file_paths = sys.argv[1:]
  
  try:
    config = find_macro_values(file_paths)
    generate_gdbinit(config)
  except (FileNotFoundError, ValueError, Exception) as e:
    print(f"Error: {e}")
    sys.exit(1)

if __name__ == '__main__':
  main()