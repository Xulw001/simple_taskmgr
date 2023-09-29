#ifndef _TASKMGR_PROC_H
#define _TASKMGR_PROC_H
#ifdef __linux__
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "resource.h"

namespace proc {
#define PROCPATHLEN 64  // must hold /proc/2000222000/task/2000222000/cmdline

using namespace common;
using scope::ScopedDir;

template <typename T>
using ProcReader = void (*)(const common::int64_t& pid, T* table);

class ProcHelper {
 public:
  template <typename T>
  static bool ReadProc(ProcReader<T> function, T* table) {
    ScopedDir proc(opendir("/proc"));
    if (proc) {
      struct dirent* entry;
      while ((entry = readdir(proc)) != NULL) {
        if (entry->d_type == DT_DIR) {
          bool proc_match = true;
          for (int i = 0; entry->d_name[i] != 0x00; i++) {
            if (entry->d_name[i] < '0' || entry->d_name[i] > '9') {
              proc_match = false;
              break;
            }
          }
          if (proc_match) {
            common::int64_t pid = atoll(entry->d_name);
            function(pid, table);
          }
        }
      }
      return true;
    }
    return false;
  }
};

}  // namespace proc
#endif
#endif