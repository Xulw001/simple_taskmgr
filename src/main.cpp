#include <locale.h>
#include <stdio.h>

#include "proc.hpp"
#ifdef __linux__
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#else
#include <mutex>
#include <thread>

#include "common/resource.h"
#endif
struct proc_flg {
  int interval;
  int cmd;
} flg = {-1, 0};

bool PraseOpt(int argc, char* argvs[]) {
  for (int i = 1; i < argc; i++) {
    if (*argvs[i] != '-') {
      return false;
    }
    switch (argvs[i][1]) {
      case 'c':
        flg.cmd = 1;
        break;
      case 'i':
        flg.interval = atoi(argvs[i + 1]);
        break;
      default:
        break;
    }
  }
  return true;
}

bool InitConsole();
bool ShowWindow(const proc::ProcTable& table);

int main(int argc, char* argvs[]) {
  if (!PraseOpt(argc, argvs)) {
    return -1;
  }

  if (!InitConsole()) {
    return -1;
  }

  proc::ProcTable table;
  proc::ProcLoader::TraverseProc(&table);
#ifdef __linux__
  usleep(150 * 1000);
#else
  Sleep(150);
#endif

  do {
    proc::ProcLoader::TraverseProc(&table);
    if (!ShowWindow(table)) {
      break;
    }
    table.clear();

  } while (true);

  return 0;
}

char* header = {"  PID    USER       RSS      %CPU  %MEM  NAME"};
char* col_fmt = {"  %-6d %-10s %-8d  %2.1f   %2.1f  %s"};
#ifdef __linux__

common::int16_t height = 0;
common::int16_t width = 0;
common::int32_t begin = 0;

bool InitConsole() {
  printf("\x1b[2J");
  printf("\x1b[?25l");

  struct winsize size;
  if (ioctl(STDIN_FILENO, TIOCGWINSZ, &size) == -1) {
    return false;
  }

  width = size.ws_col;
  height = size.ws_row;

  struct termios setting;
  tcgetattr(STDIN_FILENO, &setting);
  setting.c_lflag &= ~ECHO;
  tcsetattr(STDIN_FILENO, TCSANOW, &setting);

  return true;
}

bool ResetConsole() {
  printf("\x1b[1B\x1b[1G");
  printf("\x1b[?25h");

  struct termios setting;
  tcgetattr(STDIN_FILENO, &setting);
  setting.c_lflag |= ECHO;
  tcsetattr(STDIN_FILENO, TCSANOW, &setting);
}

void move(int y) { printf("\x1b[%d;1H", y); }

void fill(const char* str, common::ulong len) {
  char line[1024] = {0};
  memset(line, ' ', width - len);
  printf("%s%s", str, line);
}

void print(const char* str) { printf("%s\x1b[K", str); }

bool IoWait(common::ulong sec, common::ulong usec) {
  fd_set io;
  FD_SET(STDIN_FILENO, &io);
  timeval timeout = {sec, usec};
  if (select(STDIN_FILENO + 1, &io, NULL, NULL, &timeout) > 0) {
    return true;
  }
  return false;
}

bool ShowWindow(const proc::ProcTable& table) {
  const common::int16_t row = 2;
  printf("\x1b[2;1H");
  printf("\x1b[30;47m");
  fill(header, strlen(header));
  printf("\x1b[39;49m");

  int j = (begin >= table.size()) ? table.size() - 1 : begin;
  char lines[1024];
  for (int16_t line = row + 1; line < height; line++, j++) {
    move(line);
    if (j >= table.size()) {
      print("");
    } else {
      std::string user = table[j].username;
      if (user.size() > 8) {
        user = user.substr(0, 8) + "..";
      }
      common::ulong length =
          sprintf(lines, col_fmt, (int32_t)table[j].pid, user.c_str(),
                  (int32_t)table[j].memory, table[j].cpu_usage,
                  table[j].mem_usage, table[j].name.c_str());
      print(lines);
    }
  }

  if (IoWait(1, 500000)) {
    char cmd[10] = {0};
    if (read(STDIN_FILENO, cmd, sizeof(cmd)) > 0) {
      switch (cmd[0]) {
        case 'q':
          ResetConsole();
          return false;
        case '\033':
          if (strcmp(cmd, "\033[B") == 0) {
            if (begin < table.size() - 1) begin++;
          } else if (strcmp(cmd, "\033[A") == 0) {
            if (begin > 0) begin--;
          }
        default:
          break;
      }
    }
  }

  return true;
}
#else

scope::ScopedHandle hOutput;
scope::ScopedHandle hInput;
common::int16_t height = 0;
common::int16_t width = 0;
common::int32_t begin = 0;

void EnableDebugPriv() {
  scope::ScopedHandle hToken;
  LUID sedebugnameValue;
  TOKEN_PRIVILEGES tkp;

  if (!OpenProcessToken(GetCurrentProcess(),
                        TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
    return;
  }

  if (!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &sedebugnameValue)) {
    return;
  }
  tkp.PrivilegeCount = 1;
  tkp.Privileges[0].Luid = sedebugnameValue;
  tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
  AdjustTokenPrivileges(hToken, FALSE, &tkp, sizeof tkp, NULL, NULL);
}

void UpdateConsoleInfo() {
  CONSOLE_SCREEN_BUFFER_INFO screen_info;
  GetConsoleScreenBufferInfo(hOutput, &screen_info);
  height = screen_info.srWindow.Bottom - screen_info.srWindow.Top + 1;
  width = screen_info.srWindow.Right - screen_info.srWindow.Left + 1;

  // set the screen size to forbidden scroll
  SetConsoleScreenBufferSize(hOutput, {width, height});
}

bool InitConsole() {
  // create new screen buffer
  hOutput = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                      CONSOLE_TEXTMODE_BUFFER, NULL);
  if (hOutput) {
    // replace STD_OUTPUT_HANDLE with hOutput
    SetConsoleActiveScreenBuffer(hOutput);

    UpdateConsoleInfo();

    hInput = GetStdHandle(STD_INPUT_HANDLE);
    FlushConsoleInputBuffer(hInput);

    EnableDebugPriv();

    return true;
  }
  return false;
}

common::int16_t key;
std::timed_mutex io_mutex;
bool bRunning = false;

void IoMonitor() {
  INPUT_RECORD record;
  common::ulong number;
  while (bRunning) {
    if (ReadConsoleInput(hInput, &record, 1, &number)) {
      if (record.EventType == KEY_EVENT && record.Event.KeyEvent.bKeyDown) {
        key = record.Event.KeyEvent.wVirtualKeyCode;
        bRunning = false;
        break;
      }
    }
  }
  io_mutex.unlock();
}

bool IoWait(common::ulong sec, common::ulong usec) {
  if (!bRunning) {
    bRunning = true;
    io_mutex.lock();
    std::thread iowait(IoMonitor);
    if (iowait.joinable()) {
      iowait.detach();
    }
  }

  if (io_mutex.try_lock_for(std::chrono::microseconds(sec * 1000000 + usec))) {
    io_mutex.unlock();
  }

  return (bRunning == false);
}

bool ResetConsole() {
  return SetConsoleActiveScreenBuffer(GetStdHandle(STD_OUTPUT_HANDLE));
}

void Clear(COORD target) {
  common::ulong length = 0;
  FillConsoleOutputCharacterA(hOutput, 0x20, width + 1, target, &length);
}

void Print(const char* str, common::ulong len, COORD target) {
  common::ulong length = 0;
  Clear(target);
  WriteConsoleOutputCharacterA(hOutput, str, len, target, &length);
}

bool ShowWindow(const proc::ProcTable& table) {
  Clear({0, 0});
  Print(header, strlen(header), {0, 1});

  int j = (begin >= table.size()) ? table.size() - 1 : begin;
  char lines[1024];
  for (int16_t line = 2; line < height; line++, j++) {
    if (j >= table.size()) {
      Clear({0, line});
    } else {
      std::string user = table[j].username;
      if (user.size() > 8) {
        user = user.substr(0, 8) + "..";
      }
      common::ulong length =
          sprintf_s(lines, col_fmt, (int32_t)table[j].pid, user.c_str(),
                    (int32_t)table[j].memory, table[j].cpu_usage,
                    table[j].mem_usage, table[j].name.c_str());
      Print(lines, length, {0, line});
    }
  }

  if (IoWait(1, 500000)) {
    switch (key) {
      case VK_ESCAPE:  // esc
      case 0x51:       // Q
        ResetConsole();
        return false;
      case VK_DOWN:
        if (begin < table.size() - 1) begin++;
        break;
      case VK_UP:
        if (begin > 0) begin--;
        break;
      default:
        break;
    }
  }

  UpdateConsoleInfo();

  return true;
}
#endif
