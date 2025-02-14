#ifndef _TASKMGR_CPU_H
#define _TASKMGR_CPU_H
#include "common.h"

namespace proc {
using common::int32_t;
using common::int64_t;

struct CpuTime {
  int64_t u_time;
  int64_t s_time;
  int64_t start_time;
};

class CpuHelper {
 public:
  static bool UpdateSysCpuTime();
#ifdef __linux__
  static bool GetCpuUsageByPid(const int64_t &pid, float *cpu_usage);

 private:
  void InitHertz();
  static bool GetSysCpuTime(double *uptime);
  static bool GetCpuTime(const int64_t &pid, CpuTime *cpu_time);
#elif defined(_WIN32)
  static bool GetCpuUsageByPid(const int64_t &pid, float *cpu_usage, const HANDLE &hProcess);

 private:
  static bool GetSysCpuTime(int64_t *uptime);
  static bool GetCpuTime(const HANDLE &hProcess, CpuTime *cpu_time);
#endif
};

}  // namespace proc
#endif