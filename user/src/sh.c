#include "usys.h"
#include "str.h"

enum command {
  CMD_CD,
  CMD_LS,
  CMD_OTHER,
};

static char epath[32] = "/bin/";
static char input[128];
static int pid, status, envsz;
static enum command cmd;

/*
  解析shell命令,返回false则为非法格式或非法命令
  若为true,parse会设置epath (程序路径), *op (选项描述段), cmd (命令类型)
*/
static bool
parse(char** op)
{
  *op = NULL;
  // >格式检查
  if (*input == ' ')
    return false;
  bool first = true;
  char* s = input + 1;
  while (*s) {
    if (*s == ' ' && *(s - 1) == ' ')
      return false;
    if (*s == ' ' && first) {
      *op = s + 1;
      *s = 0;
      first = false;
    }
    ++s;
  }
  if (*(s - 1) == ' ')
    return false;
  // <不允许有多余的空格

  if (strcmp(input, "cd") == 0)
    cmd = CMD_CD;
  else if (strcmp(input, "ls") == 0)
    cmd = CMD_LS;
  else {
    cmd = CMD_OTHER;
    strcpy(epath + envsz, input);
  }
  return true;
}

void
main(void)
{
  envsz = strlen(epath);
  open("/dev/console", O_RDONLY);
  open("/dev/console", O_WRONLY);

  while (1) {
    write(STDOUT, "$>", 2);
    int n = read(STDIN, input, sizeof(input) - 1);
    if (n <= 0 || *input == '\n' || *input == '\x04') {
      if (*input == '\x04')
        write(STDOUT, "\n", 1);
      continue;
    }
    input[n - 1] = 0; // 去除\n

    char* option;
    if (! parse(&option)) {
      write(STDOUT, "invalid cmd\n", 12);
      continue;
    }

    if (cmd == CMD_CD) {
      if (! option || *option == '\0' || chdir(option))
        write(STDOUT, "invalid path\n", 13);
    } else if (cmd == CMD_LS) {
      if (option && *option)
        write(STDOUT, "invalid opt\n", 12);
      else
        ls();
    } else {
      pid = fork();
      if (pid == 0) {
        exec(epath, option);
        write(STDOUT, "invalid cmd\n", 12);
        exit(1);
      }
      wait(&status);
    }
  }
}