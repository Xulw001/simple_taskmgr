#include "module.h"

#include "resource.h"
#ifdef __linux__
#include <stdio.h>

#include <unordered_set>

#include "proc.h"

using scope::ScopedFile;
#else
#include <Windows.h>
#include <tlHelp32.h>

#include "tool.h"

using scope::ScopedHandle;
#endif

namespace proc {

bool GetModulesByPid(const int32_t &pid, ModuleTable *ptable) {
#ifdef __linux__
  char map_path[PROCPATHLEN];
  sprintf(map_path, "/proc/%d/maps", pid);
  ScopedFile fp(fopen(map_path, "r"));
  if (fp) {
    char line[1024];
    char filename[1024];
    std::unordered_set<std::string> module_sets;
    while (fgets(line, sizeof(line), fp)) {
      sscanf(line, "%*s %*s %*s %*s %*d %s", filename);
      if (filename[0] == '/') {
        module_sets.emplace(filename);
      }
    }
    fclose(fp);

    for (std::unordered_set<std::string>::const_iterator itr =
             module_sets.begin();
         itr != module_sets.end(); itr++) {
      ptable->push_back(*itr);
    }
    return true;
  }
  return false;
#else
  ScopedHandle hSnapshot(
      CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid));
  if (hSnapshot) {
    MODULEENTRY32 md32;
    md32.dwSize = sizeof(md32);
    if (Module32First(hSnapshot, &md32)) {
      do {
        std::string filepath;
        common::W2A(md32.szExePath, &filepath);
        ptable->push_back(filepath);
      } while (Module32Next(hSnapshot, &md32));
    }

    return true;
  }
  return false;
#endif
}
}  // namespace proc