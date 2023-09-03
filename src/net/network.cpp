#include <locale.h>
#include <stdio.h>

#include "net.h"

struct network_flg {
  int proc;
} flg = {0};

bool PraseOpt(int argc, char* argvs[]) {
  for (int i = 0; i < argc; i++) {
    if (*argvs[i] != '-') {
      return false;
    }
    switch (argvs[i][1]) {
      case 'p':
        flg.proc = 1;
        break;
      default:
        break;
    }
  }
  return true;
}

int Show(const net::NetWorkTable& table);

int net_main(int argc, char* argvs[]) {
  if (PraseOpt(argc, argvs)) {
    return -1;
  }

  net::NetWorkHelper net_helper;
  net::NetWorkTable table;
  bool ret = true;
  if (flg.proc) {
    ret = net_helper.GetConnectionWithPid(&table);
  } else {
    ret = net_helper.GetConnection(&table);
  }

  return Show(table);
}

char* header[] = {
    "  协议              本地地址            外部地址            状态",
    "  协议              本地地址            外部地址            状态      进程"};
char* fmt[] = {"  %-6s %-24s:d %-24s:d %-16s", "  %-6s %-24s:d %-24s:d %-16s %8d"};
char* protocol[] = {"TCP", "UDP"};
char* netstatus[] = {"LISTENING", "ESTABLISHED", "TIME_WAIT", "CLOSE_WAIT",
                     "NO_STATUS"};

int Show(const net::NetWorkTable& table) {
  printf("%s", header[flg.proc]);
  char* using_fmt = fmt[flg.proc];
  for (size_t i = 0; i < table.size(); i++) {
    net::NetWork info = table[i];
    if (flg.proc) {
      printf(using_fmt, protocol[info.protocol], info.local_host, header[2],
             header[3], header[4]);
    } else {
      printf(using_fmt, protocol[info.protocol], info.local_host, header[2],
             header[3]);
    }
  }
}