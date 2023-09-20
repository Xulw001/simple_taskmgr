#include <locale.h>
#include <stdio.h>

#include "net.h"

struct network_flg {
  int proc;
} flg = {0};

bool PraseOpt(int argc, char* argvs[]) {
  for (int i = 1; i < argc; i++) {
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

void Show(const net::NetWorkTable& table);

int main(int argc, char* argvs[]) {
  if (!PraseOpt(argc, argvs)) {
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

  if (ret) {
    Show(table);
  }

  return 0;
}

char* header[] = {"PROTO", "LOCAL-ADDR", "REMOTE-ADDR", "STATUS", "PID"};
char header_fmt[] = {"  %%-6s%%-%ds %%-%ds %%-16s% %6s\n"};

char* col_fmt[] = {"  %-6s", "%s:%d", "%s:%d", "%-16s", "%6ld", "\n"};

char* protocol[] = {"TCP", "UDP"};
char* netstatus[] = {"LISTENING", "ESTABLISHED", "TIME_WAIT", "CLOSE_WAIT",
                     "NO_STATUS"};

void Show(const net::NetWorkTable& table) {
  int local = 0, remote = 0;
  for (size_t i = 0; i < table.size(); i++) {
    net::NetWork info = table[i];
    if (info.local_host.size() > local) {
      local = info.local_host.size();
    }

    if (info.remote_host.size() > remote) {
      remote = info.remote_host.size();
    }
  }

  local += 6;
  remote += 6;

  char column[1024];
  sprintf(column, header_fmt, local, remote);

  if (flg.proc) {
    printf(column, header[0], header[1], header[2], header[3], header[4]);
  } else {
    printf(column, header[0], header[1], header[2], header[3], "");
  }

  int col = 0;
  for (size_t i = 0; i < table.size(); i++) {
    net::NetWork info = table[i];
    switch (col) {
      case 0:
        printf(col_fmt[col++], protocol[info.protocol]);
      case 1: {
        int len = sprintf(column, col_fmt[col++], info.local_host.c_str(),
                          info.local_port);
        printf("%s", column);

        sprintf(column, " %%%ds", local - len);
        printf(column, "");
      }
      case 2:
        if (info.protocol == net::TCP) {
          int len = sprintf(column, col_fmt[col], info.remote_host.c_str(),
                            info.remote_port);
          printf("%s", column);

          sprintf(column, " %%%ds", remote - len);
        } else {
          sprintf(column, " %%%ds", remote);
        }
        printf(column, "");
        col++;
      case 3:
        printf(col_fmt[col++], netstatus[info.status]);
      case 4:
        if (flg.proc) {
          printf(col_fmt[col++], info.pid);
        }
      case 5:
        printf("\n");
      default:
        col = 0;
        break;
    }
  }
}