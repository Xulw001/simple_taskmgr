#ifndef _TASKMGR_PROC_HPP
#define _TASKMGR_PROC_HPP
#include "module.h"
#include "net.h"

namespace proc {

using net::NetWorkTable;

struct Proc {
  common::int64_t pid;
  std::string name;
  std::string username;
  float cpu_usage;
  common::int64_t memory;
  float mem_usage;
  std::string startparamater;
};

typedef std::vector<Proc> ProcTable;

struct ProcDetail {
  Proc info;
  ModuleTable modules;
  NetWorkTable networks;
};

class ProcLoader {
 public:
  static bool TraverseProc(ProcTable *procs);
  static bool GetProcByPid(const common::int64_t &pid, ProcDetail *proc);

 private:
#ifdef __linux__
  static void ProcReader(const common::int64_t &pid, ProcTable *ptable);
#else
  static bool GetDetailByPid(Proc *proc);
#endif
};

}  // namespace proc
#endif