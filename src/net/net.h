#ifndef _TASKMGR_NETWORK_H
#define _TASKMGR_NETWORK_H
#include <string>
#include <vector>

#include "common.h"

namespace net {
using common::int32_t;
using common::int64_t;
using common::ulong;

enum NetStatus {
  LISTENING,
  ESTABLISHED,
  TIME_WAIT,
  CLOSE_WAIT,
  NO_STATUS,
};

enum NetProtocol {
  TCP,
  UDP,
};

struct NetWork {
  NetStatus status;
  NetProtocol protocol;
  std::string local_host;
  int32_t local_port;
  std::string remote_host;
  int32_t remote_port;
  union {
    int64_t pid;
    int64_t inode;
  };
};

typedef std::vector<NetWork> NetWorkTable;

class NetWorkHelper {
 public:
  static bool GetConnection(NetWorkTable *ptable);
  static bool GetConnectionWithPid(NetWorkTable *ptable);
};

}  // namespace net
#endif