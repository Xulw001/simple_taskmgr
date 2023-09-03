#ifndef _TASKMGR_MODULE_H
#define _TASKMGR_MODULE_H
#include <string>
#include <vector>

namespace proc {
typedef std::vector<std::string> ModuleTable;

static bool GetModulesByPid(const int32_t &pid, ModuleTable *ptable);
}  // namespace proc
#endif