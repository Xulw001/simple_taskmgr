#include "cpu.h"

#include "resource.h"

#ifdef __linux__
#include <stdio.h>
#include <unistd.h>

#include "proc.h"
#else
#include <Windows.h>
#endif
#include <unordered_map>

namespace proc {

using scope::ScopedFile;

typedef std::unordered_map<int64_t, CpuTime> CpuTimeMap;

#ifdef __linux__
int32_t hertz_ = 0;
double uptime_save_ = 0.0;
float frame_etscale_ = 0.0f;
#elif defined(_WIN32)
int64_t systime_save_ = 0;
int64_t frame_etscale_ = 0;
#endif
CpuTimeMap time_map;

bool CpuHelper::UpdateSysCpuTime() {
#ifdef __linux__
  if (!hertz_) {
    hertz_ = sysconf(_SC_CLK_TCK);
  }

  double uptime_cur;
  bool ret = GetSysCpuTime(&uptime_cur);
  if (!ret) {
    return false;
  }

  float et = uptime_cur - uptime_save_;
  if (et < 0.01) {
    et = 0.005;
  }
  uptime_save_ = uptime_cur;
  frame_etscale_ = 1000.0f / ((float)hertz_ * (float)et * 1 /*cpu*/);
  return true;
#else
  int64_t systime_cur;
  bool ret = GetSysCpuTime(&systime_cur);
  if (!ret) {
    return false;
  }

  int64_t et = systime_cur - systime_save_;
  if (et == 0) {
    et = 1;
  }
  systime_save_ = systime_cur;
  frame_etscale_ = 1000.0f / et;
  return true;
#endif
}

#ifdef __linux__
bool CpuHelper::GetCpuUsageByPid(const int64_t &pid, int32_t *cpu_usage) {
#else
bool CpuHelper::GetCpuUsageByPid(const HANDLE &pid, int32_t *cpu_usage) {
#endif
  CpuTime cputime_cur;
  bool ret = GetCpuTime(pid, &cputime_cur);
  if (!ret) {
    return false;
  }

  CpuTimeMap::iterator find = time_map.find((int64_t)pid);
  if (find == time_map.end()) {
    *cpu_usage = -1;
    time_map.emplace((int64_t)pid, cputime_cur);
  } else {
    int64_t time_diff = (cputime_cur.s_time + cputime_cur.u_time) -
                        (find->second.s_time + find->second.u_time);
    *cpu_usage = time_diff * frame_etscale_;
    find->second = cputime_cur;
  }
  return true;
}  // namespace proc

#ifdef __linux__
bool CpuHelper::GetCpuTime(const int64_t &pid, CpuTime *cpu_time) {
  char stat_path[PROCPATHLEN];
  sprintf(stat_path, "/proc/%d/stat", pid);
  ScopedFile fp(fopen(stat_path, "r"));
  if (fp) {
    char line[1024];
    if (fgets(line, sizeof(line), fp)) {
      int64_t u_time, s_time, wait_u_time, wait_s_time, start_time;
      sscanf(line,
             "%*d %*s %*c %*d %*d %*d %*d %*d "
             "%*lu %*lu %*lu %*lu %*lu"
             "%llu %llu %llu %llu"
             "%*ld %*ld "
             "%*d "
             "%*ld "
             "%llu ", /* start_time */
             &u_time, &s_time, &wait_u_time, &wait_s_time, &start_time);

      cpu_time->s_time = s_time;
      cpu_time->u_time = u_time;
      cpu_time->start_time = start_time;
    }
    return true;
  }
  return false;
#else
bool CpuHelper::GetCpuTime(const HANDLE &hProcess, CpuTime *cpu_time) {
  if (hProcess) {
    FILETIME start_time, exit_time, s_time, u_time;
    if (!GetProcessTimes(hProcess, &start_time, &exit_time, &s_time, &u_time)) {
      return false;
    }

#define FileTimeToInt64(time, res)          \
  {                                         \
    ULARGE_INTEGER large;                   \
    large.LowPart = (time).dwLowDateTime;   \
    large.HighPart = (time).dwHighDateTime; \
    (res) = large.QuadPart;                 \
  }

    FileTimeToInt64(s_time, cpu_time->s_time);
    FileTimeToInt64(u_time, cpu_time->u_time);
    FileTimeToInt64(start_time, cpu_time->start_time);

    return true;
  }
  return false;
#endif
}

#ifdef __linux__
bool CpuHelper::GetSysCpuTime(double *uptime) {
  ScopedFile fp(fopen("/proc/uptime", "r"));
  if (fp) {
    char line[1024];
    fgets(line, sizeof(line), fp);
    sscanf(line, "%lf", uptime);
    return true;
  }
  return false;
}
#else
bool CpuHelper::GetSysCpuTime(int64_t *time) {
  FILETIME idleTime, kernelTime, userTime;
  if (!GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
    return false;
  }

  ULARGE_INTEGER cpu_time;
  cpu_time.LowPart = kernelTime.dwLowDateTime + userTime.dwLowDateTime;
  cpu_time.HighPart = kernelTime.dwHighDateTime + userTime.dwHighDateTime;

  *time = cpu_time.QuadPart;
  return true;
}
#endif
}  // namespace proc