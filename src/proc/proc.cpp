#include "proc.hpp"

#ifdef __linux__
#include <pwd.h>
#include <stdio.h>
#include <string.h>

#include "proc.h"
using scope::ScopedFile;
#else
#include <Windows.h>
#include <tlHelp32.h>
#define FORMAT_AVOID
#include <Psapi.h>
#include <winternl.h>

#include "resource.h"

using scope::ScopedHandle;
using scope::ScopedModule;
using scope::ScopedPtr;
#endif

#include "cpu.h"
#include "tool.h"

namespace proc {

bool ProcLoader::TraverseProc(ProcTable *procs) {
  CpuHelper::UpdateSysCpuTime();
#ifdef __linux__
  return ProcHelper::ReadProc(ProcLoader::ProcReader, procs);
#else
  ScopedHandle hSnapshot(
      CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS | TH32CS_SNAPTHREAD, 0));
  if (hSnapshot) {
    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(pe32);
    if (Process32First(hSnapshot, &pe32)) {
      do {
        std::string path;
        common::W2A(pe32.szExeFile, &path);

        Proc info;
        info.pid = pe32.th32ProcessID;
        info.name = path;
        if (GetDetailByPid(&info)) {
          procs->emplace_back(info);
        }
      } while (Process32Next(hSnapshot, &pe32));
    }

    return true;
  }

  return false;
#endif
}

#ifdef __linux__
bool GetStatusInfoByPid(const int64_t &memory, float *mem_usage) {
  const char *meminfo_path = "/proc/meminfo";
  ScopedFile fp(fopen(meminfo_path, "r"));
  if (fp) {
    char line[4096];
    int64_t total_memory = 0;
    while (fgets(line, sizeof(line), fp)) {
      if (strncmp(line, "MemTotal:", 9) == 0) {
        sscanf(line, "%*s:%lld", &total_memory);
        break;
      }
    }
    *mem_usage = memory * 1024 * 100.0f / total_memory;
    return true;
  }
  return false;
}

bool GetStatusInfoByPid(const int64_t &pid, std::string *sname,
                        std::string *username, int64_t *memory) {
  char status_path[PROCPATHLEN];
  sprintf(status_path, "/proc/%lld/status", pid);

  ScopedFile fp(fopen(status_path, "r"));
  if (fp) {
    char line[4096];
    while (fgets(line, sizeof(line), fp)) {
      if (strncmp(line, "Name:", 5) == 0) {
        char name[PROCPATHLEN];
        sscanf(line, "%*s:%s", name);
        sname->assign(name);
      } else if (strncmp(line, "Uid:", 4) == 0) {
        int32_t uid = 0;
        sscanf(line, "%*s:%d", &uid);

        struct passwd *_passwd;
        _passwd = getpwuid(uid);
        if (_passwd) {
          username->assign(_passwd->pw_name);
        }
      } else if (strncmp(line, "VmRSS:", 6) == 0) {
        sscanf(line, "%*s:%lld", memory);
        break;
      }
    }
    return true;
  }
  return false;
}

bool GetStartInfoByPid(const int64_t &pid, std::string *startparamater) {
  char cmd_path[PROCPATHLEN];
  sprintf(cmd_path, "/proc/%lld/cmdline", pid);

  ScopedFile fp(fopen(cmd_path, "r"));
  if (fp) {
    char line[4096];
    if (fgets(line, sizeof(line), fp)) {
      startparamater->assign(line);
      int32_t offset = startparamater->size() + 1;
      while (line[offset]) {
        startparamater->append(" ");
        startparamater->append(line + offset);
        offset += startparamater->size() + 1;
      }
    }
    return true;
  }
  return false;
}

void ProcLoader::ProcReader(const int64_t &pid, ProcTable *ptable) {
  Proc proc;
  proc.pid = pid;

  if (!GetStatusInfoByPid(pid, &proc.name, &proc.username, &proc.memory)) {
    return;
  }

  if (!CpuHelper::GetCpuUsageByPid(pid, &proc.cpu_usage)) {
    return;
  }

  if (!GetStartInfoByPid(pid, &proc.startparamater)) {
    return;
  }

  ptable->emplace_back(proc);
}
#else
bool GetUserNameByPid(const HANDLE &hProcess, std::string *username) {
  bool ret = false;
  if (hProcess) {
    ScopedHandle pToken;
    if (OpenProcessToken(hProcess, TOKEN_QUERY, &pToken)) {
      DWORD token_size = 0;
      SID_NAME_USE sn;
      ScopedPtr<TOKEN_USER> p_token_user;
      if (!GetTokenInformation(pToken, TokenUser, p_token_user, token_size,
                               &token_size)) {
        if (ERROR_INSUFFICIENT_BUFFER == GetLastError()) {
          p_token_user = (PTOKEN_USER) new char[token_size];
          if (p_token_user) {
            if (GetTokenInformation(pToken, TokenUser, p_token_user, token_size,
                                    &token_size)) {
              TCHAR szUserName[MAX_PATH] = {0};
              DWORD dwUserSize = MAX_PATH;
              TCHAR szDomain[MAX_PATH] = {0};
              DWORD dwDomainSize = MAX_PATH;
              if (LookupAccountSid(NULL, ((PTOKEN_USER)p_token_user)->User.Sid,
                                   szUserName, &dwUserSize, szDomain,
                                   &dwDomainSize, &sn)) {
                common::W2A(szUserName, username);
                ret = true;
              }
            }
          }
        }
      }
    }
  }

  return ret;
}

bool GetMemByPid(const HANDLE &hProcess, int64_t *mem_used_size,
                 float *mem_usage) {
  SYSTEM_INFO si;
  GetSystemInfo(&si);
  if (hProcess) {
    PSAPI_WORKING_SET_INFORMATION worksets;
    if (!QueryWorkingSet(hProcess, &worksets, sizeof(worksets))) {
      if (GetLastError() == ERROR_BAD_LENGTH) {
        size_t length =
            sizeof(PSAPI_WORKING_SET_INFORMATION) +
            sizeof(PSAPI_WORKING_SET_BLOCK) * (worksets.NumberOfEntries + 64);

        ScopedPtr<PSAPI_WORKING_SET_INFORMATION> pworksets(
            (PPSAPI_WORKING_SET_INFORMATION) new char[length]);
        if (QueryWorkingSet(hProcess, pworksets, length)) {
          *mem_used_size = 0;
          for (int i = 0; i < pworksets->NumberOfEntries; i++) {
            if (pworksets->WorkingSetInfo[i].Flags &&
                pworksets->WorkingSetInfo[i].Shared == 0) {
              *mem_used_size += si.dwPageSize;
            }
          }
        }
      } else {
        return false;
      }
    }

    MEMORYSTATUSEX mem_info;
    mem_info.dwLength = sizeof(mem_info);
    if (!GlobalMemoryStatusEx(&mem_info)) {
      return false;
    }

    *mem_usage = (*mem_used_size) * 100.0f / mem_info.ullTotalPhys;
    *mem_used_size = (*mem_used_size) / 1024;

    return true;
  }

  return false;
}

typedef NTSTATUS(NTAPI *NT_QUERY_INFORMATION_PROCESS)(HANDLE, PROCESSINFOCLASS,
                                                      PVOID, ULONG, PULONG);
bool GetStartInfoByPid(const HANDLE &hProcess, std::string *startparamater) {
  bool result = false;
  ScopedModule m_ntdll(LoadLibrary(L"Ntdll"));
  if (m_ntdll) {
    NT_QUERY_INFORMATION_PROCESS pNtQueryInformationProcess =
        (NT_QUERY_INFORMATION_PROCESS)GetProcAddress(
            m_ntdll, "NtQueryInformationProcess");
    if (pNtQueryInformationProcess) {
      ScopedPtr<WCHAR> buffer;
      do {
        PROCESS_BASIC_INFORMATION pbi = {0};
        if (pNtQueryInformationProcess(
                hProcess, ProcessBasicInformation, (PVOID)&pbi,
                sizeof(PROCESS_BASIC_INFORMATION), NULL)) {
          break;
        }

        if (NULL == pbi.PebBaseAddress) {
          break;
        }

        PEB peb;
        SIZE_T dwDummy;
        if (!ReadProcessMemory(hProcess, pbi.PebBaseAddress, &peb, sizeof(peb),
                               &dwDummy)) {
          break;
        }

        RTL_USER_PROCESS_PARAMETERS para;
        if (!ReadProcessMemory(hProcess, peb.ProcessParameters, &para,
                               sizeof(para), &dwDummy)) {
          break;
        }

        DWORD dwSize = para.CommandLine.Length;
        buffer = new WCHAR[dwSize / sizeof(WCHAR) + 1];
        buffer[dwSize / sizeof(WCHAR)] = 0x00;
        if (!ReadProcessMemory(hProcess, para.CommandLine.Buffer, buffer,
                               dwSize, &dwDummy)) {
          break;
        }
        common::W2A(std::wstring(buffer), startparamater);

        return true;
      } while (false);
    }
  }
  return false;
}

bool ProcLoader::GetDetailByPid(Proc *proc) {
  ScopedHandle hProcess(OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                                    FALSE, proc->pid));
  if (hProcess) {
    if (!GetUserNameByPid(hProcess, &proc->username)) {
      return false;
    }
    if (!GetStartInfoByPid(hProcess, &proc->startparamater)) {
      return false;
    }
    if (!GetMemByPid(hProcess, &proc->memory, &proc->mem_usage)) {
      return false;
    }
    if (!CpuHelper::GetCpuUsageByPid(proc->pid, &proc->cpu_usage, hProcess)) {
      return false;
    }
    return true;
  }
  return false;
}
#endif

}  // namespace proc