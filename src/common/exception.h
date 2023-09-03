#ifndef _TASKMGR_EXCEPTION_H
#define _TASKMGR_EXCEPTION_H
#include "common.h"
namespace except {
using common::int32_t;

class Exception {
 public:
  Exception(const char* what, int32_t system_code)
      : what_(what), system_code_(system_code) {
    ;
  }

  const char* what() const { return what_; }
  int32_t system_code() const { return system_code_; }

 private:
  const char* what_;
  int32_t system_code_;
};
}  // namespace except

#endif