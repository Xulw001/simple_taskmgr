#include "net.h"

#ifdef __linux__
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>

#include <unordered_map>

#include "proc.h"
#else
#include <WS2tcpip.h>
#include <WinSock2.h>
#include <iphlpapi.h>

#include "resource.h"
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#endif

namespace net {

using scope::ScopedFile;
using scope::ScopedPtr;

#ifdef __linux__
NetStatus tcp_state[] = {NO_STATUS,  ESTABLISHED, NO_STATUS, NO_STATUS,
                         NO_STATUS,  NO_STATUS,   TIME_WAIT, NO_STATUS,
                         CLOSE_WAIT, NO_STATUS,   LISTENING, NO_STATUS};

bool DoGetTcpInfo(NetWorkTable *ptable) {
  const char *tcp_file[] = {"/proc/net/tcp", "/proc/net/tcp6"};
  ScopedFile fp;
  for (int i = 0; i < 2; i++) {
    fp = fopen(tcp_file[i], "r");
    if (fp) {
      char line[1024];
      while (fgets(line, sizeof(line), fp)) {
        unsigned long inode;
        int num, local_port, remote_port, state, ;
        char rem_addr[128], local_addr[128];

        num = sscanf(line,
                     "%*d: %64[0-9A-Fa-f]:%X %64[0-9A-Fa-f]:%X %X %*lX:%*lX "
                     "*%X:%*lX %*lX %*d %*d %lu %*s\n",
                     local_addr, &local_port, rem_addr, &remote_port, &state,
                     &inode);
        if (num < 11) {
          continue;
        }

        NetWork netInfo;
        if (strlen(local_addr) > 8) {
          char addr6[INET6_ADDRSTRLEN];
          struct in6_addr in6;

          sscanf(local_addr, "%08X%08X%08X%08X", &in6.s6_addr32[0],
                 &in6.s6_addr32[1], &in6.s6_addr32[2], &in6.s6_addr32[3]);
          inet_ntop(AF_INET6, &in6, addr6, sizeof(addr6));
          netInfo.local_host = addr6;

          sscanf(rem_addr, "%08X%08X%08X%08X", &in6.s6_addr32[0],
                 &in6.s6_addr32[1], &in6.s6_addr32[2], &in6.s6_addr32[3]);
          inet_ntop(AF_INET6, &in6, addr6, sizeof(addr6));
          netInfo.remote_host = addr6;
        } else {
          char addr[INET_ADDRSTRLEN];
          struct in_addr in;

          sscanf(local_addr, "%X", &in.s_addr);
          inet_ntop(AF_INET, &in, addr, sizeof(addr));
          netInfo.local_host = addr;

          sscanf(rem_addr, "%X", &in.s_addr);
          inet_ntop(AF_INET, &in, addr, sizeof(addr));
          netInfo.remote_host = addr;
        }

        netInfo.local_port = local_port;
        netInfo.remote_port = remote_port;
        netInfo.status = tcp_state[state];
        netInfo.inode = inode;
        netInfo.protocol = TCP;
        ptable->push_back(netInfo);
      }
    }
  }
  return true;
}

bool DoGetUdpInfo(NetWorkTable *ptable) {
  const char *udp_file[] = {"/proc/net/udp", "/proc/net/udp6"};
  ScopedFile fp;
  for (int i = 0; i < 2; i++) {
    fp = fopen(udp_file[i], "r");
    if (fp) {
      char line[1024];
      while (fgets(line, sizeof(line), fp)) {
        unsigned long inode;
        int num, local_port, remote_port, state;
        char rem_addr[128], local_addr[128];

        num = sscanf(line,
                     "%*d: %64[0-9A-Fa-f]:%X %64[0-9A-Fa-f]:%X %X %*lX:%*lX "
                     "%*X:%*lX %*lX %*d %*d %lu %*s\n",
                     local_addr, &local_port, rem_addr, &remote_port, &state,
                     &inode);
        if (num < 10) {
          continue;
        }

        NetWork netInfo;
        if (strlen(local_addr) > 8) {
          char addr6[INET6_ADDRSTRLEN];
          struct in6_addr in6;

          sscanf(local_addr, "%08X%08X%08X%08X", &in6.s6_addr32[0],
                 &in6.s6_addr32[1], &in6.s6_addr32[2], &in6.s6_addr32[3]);
          inet_ntop(AF_INET6, &in6, addr6, sizeof(addr6));
          netInfo.local_host = addr6;
        } else {
          char addr[INET_ADDRSTRLEN];
          struct in_addr in;

          sscanf(local_addr, "%X", &in.s_addr);
          inet_ntop(AF_INET, &in, addr, sizeof(addr));
          netInfo.local_host = addr;
        }

        netInfo.local_port = local_port;
        netInfo.status = tcp_state[state];
        netInfo.inode = inode;
        netInfo.protocol = UDP;
        ptable->push_back(netInfo);
      }
    }
  }
  return true;
}

void Reader(const int64_t &pid, NetWorkTable *ptable) {
  char fd_dir[PROCPATHLEN];
  sprintf(fd_dir, "/proc/%d/fd", pid);

  scope::ScopedDir dir(opendir(fd_dir));
  if (dir) {
    std::unordered_map<int32_t, int32_t> hash;
    for (size_t i = 0; i < ptable->size(); i++) {
      hash.emplace(ptable->at(i).inode, i);
    }

    struct dirent *fd;
    while ((fd = readdir(dir)) != NULL) {
      if (!isdigit(fd->d_name[0])) {
        continue;
      }
      if (strlen(fd_dir) + 1 + strlen(fd->d_name) + 1 > 4096) {
        continue;
      }

      char fd_path[PROCPATHLEN];
      sprintf(fd_path, "%s/%s", fd_dir, fd->d_name);

      char lname[30];
      int len = readlink(fd_path, lname, sizeof(lname) - 1);
      if (len == -1) {
        continue;
      }

      if (lname[len - 1] != ']') {
        continue;
      }

#define PRG_SOCKET_PFX "socket:["
#define PRG_SOCKET_PFXl (sizeof(PRG_SOCKET_PFX) - 1)
#define PRG_SOCKET_PFX2 "[0000]:"
#define PRG_SOCKET_PFX2l (sizeof(PRG_SOCKET_PFX2) - 1)
      int64_t inode = 0;
      if (memcmp(lname, PRG_SOCKET_PFX, PRG_SOCKET_PFXl) == 0) {
        inode = atoll(lname + PRG_SOCKET_PFXl);
      } else if (memcmp(lname, PRG_SOCKET_PFX2, PRG_SOCKET_PFX2l) == 0) {
        inode = atoll(lname + PRG_SOCKET_PFX2l);
      }

      std::unordered_map<int32_t, int32_t>::iterator it = hash.find(inode);
      if (it != tables.end()) {
        ptable->at(it->second).pid = pid;
      }
    }
  }
}
#endif

bool NetWorkHelper::GetConnection(NetWorkTable *ptable) {
#ifdef __linux__
  if (!DoGetTcpInfo(ptable)) {
    return false;
  }
  if (!DoGetUdpInfo(ptable)) {
    return false;
  }
  return true;
#else
  NetStatus tcp_state[] = {NO_STATUS,  NO_STATUS,   LISTENING, NO_STATUS,
                           NO_STATUS,  ESTABLISHED, NO_STATUS, NO_STATUS,
                           CLOSE_WAIT, NO_STATUS,   NO_STATUS, TIME_WAIT};

  do {
    ulong net_table_size;
    ScopedPtr<BYTE> net_table;

    char ipaddress[INET6_ADDRSTRLEN];
    int32_t ip_type[] = {AF_INET6, AF_INET};
    for (int i = 0; i < 2; i++) {
      net_table_size = 0;
      if (GetExtendedTcpTable(NULL, &net_table_size, FALSE, ip_type[i],
                              TCP_TABLE_OWNER_PID_ALL,
                              0) == ERROR_INSUFFICIENT_BUFFER) {
        net_table = new BYTE[net_table_size];
        if (net_table) {
          if (GetExtendedTcpTable(net_table, &net_table_size, FALSE, ip_type[i],
                                  TCP_TABLE_OWNER_PID_ALL, 0) == NO_ERROR) {
            if (i == 0) {
              MIB_TCP6TABLE_OWNER_PID *net_table_v6 =
                  (MIB_TCP6TABLE_OWNER_PID *)(BYTE *)net_table;
              for (ulong i = 0; i < net_table_v6->dwNumEntries; i++) {
                MIB_TCP6ROW_OWNER_PID row = net_table_v6->table[i];
                NetWork netInfo;
                netInfo.protocol = TCP;
                netInfo.pid = row.dwOwningPid;
                netInfo.status = tcp_state[row.dwState];

                // local addr info
                if (inet_ntop(AF_INET6, row.ucLocalAddr, ipaddress,
                              INET6_ADDRSTRLEN)) {
                  netInfo.local_host.append("[").append(ipaddress).append("]");
                } else {
                  netInfo.local_host = "[::]";
                }
                netInfo.local_port = ntohs(row.dwLocalPort);

                // remote addr info
                if (inet_ntop(AF_INET6, row.ucRemoteAddr, ipaddress,
                              INET6_ADDRSTRLEN)) {
                  netInfo.remote_host.append("[").append(ipaddress).append("]");
                } else {
                  netInfo.remote_host = "[::]";
                }
                netInfo.remote_port = ntohs(row.dwRemotePort);

                ptable->push_back(netInfo);
              }
            } else {
              MIB_TCPTABLE_OWNER_PID *net_table_v4 =
                  (MIB_TCPTABLE_OWNER_PID *)(BYTE *)net_table;
              for (ulong i = 0; i < net_table_v4->dwNumEntries; i++) {
                MIB_TCPROW_OWNER_PID row = net_table_v4->table[i];
                NetWork netInfo;
                netInfo.protocol = TCP;
                netInfo.pid = row.dwOwningPid;
                netInfo.status = tcp_state[row.dwState];

                // local addr info
                if (inet_ntop(AF_INET, &row.dwLocalAddr, ipaddress,
                              INET6_ADDRSTRLEN)) {
                  netInfo.local_host = ipaddress;
                } else {
                  netInfo.local_host = "0.0.0.0";
                }
                netInfo.local_port = ntohs(row.dwLocalPort);

                // remote addr info
                if (inet_ntop(AF_INET, &row.dwRemoteAddr, ipaddress,
                              INET6_ADDRSTRLEN)) {
                  netInfo.remote_host = ipaddress;
                } else {
                  netInfo.remote_host = "0.0.0.0";
                }
                netInfo.remote_port = ntohs(row.dwRemotePort);

                ptable->push_back(netInfo);
              }
            }
          }
        }
      }
    }

    for (int i = 0; i < 2; i++) {
      net_table_size = 0;
      if (GetExtendedUdpTable(NULL, &net_table_size, FALSE, ip_type[i],
                              UDP_TABLE_OWNER_PID,
                              0) == ERROR_INSUFFICIENT_BUFFER) {
        net_table = new BYTE[net_table_size];
        if (net_table) {
          if (GetExtendedUdpTable(net_table, &net_table_size, FALSE, ip_type[i],
                                  UDP_TABLE_OWNER_PID, 0) == NO_ERROR) {
            if (i == 0) {
              MIB_UDP6TABLE_OWNER_PID *net_table_v6 =
                  (MIB_UDP6TABLE_OWNER_PID *)(BYTE *)net_table;
              for (ulong i = 0; i < net_table_v6->dwNumEntries; i++) {
                MIB_UDP6ROW_OWNER_PID row = net_table_v6->table[i];
                NetWork netInfo;
                netInfo.protocol = UDP;
                netInfo.pid = row.dwOwningPid;
                netInfo.status = NO_STATUS;

                // local addr netInfo
                if (inet_ntop(AF_INET6, row.ucLocalAddr, ipaddress,
                              INET6_ADDRSTRLEN)) {
                  netInfo.local_host.append("[").append(ipaddress).append("]");
                } else {
                  netInfo.local_host = "[::]";
                }
                netInfo.local_port = ntohs(row.dwLocalPort);
                ptable->push_back(netInfo);
              }
            } else {
              MIB_UDPTABLE_OWNER_PID *net_table_v4 =
                  (MIB_UDPTABLE_OWNER_PID *)(BYTE *)net_table;
              for (ulong i = 0; i < net_table_v4->dwNumEntries; i++) {
                MIB_UDPROW_OWNER_PID row = net_table_v4->table[i];
                NetWork netInfo;
                netInfo.protocol = UDP;
                netInfo.pid = row.dwOwningPid;
                netInfo.status = NO_STATUS;

                // local addr netInfo
                if (inet_ntop(AF_INET, &row.dwLocalAddr, ipaddress,
                              INET6_ADDRSTRLEN)) {
                  netInfo.local_host = ipaddress;
                } else {
                  netInfo.local_host = "0.0.0.0";
                }
                netInfo.local_port = ntohs(row.dwLocalPort);

                ptable->push_back(netInfo);
              }
            }
          }
        }
      }
    }
  } while (false);

  return true;
#endif
}

bool NetWorkHelper::GetConnectionWithPid(NetWorkTable *ptable) {
#ifdef __linux__
  if (!DoGetTcpInfo(ptable)) {
    return false;
  }
  if (!DoGetUdpInfo(ptable)) {
    return false;
  }

  return proc::ProcHelper::ReadProc(Reader, ptable);
#else
  return GetConnection(ptable);
#endif
}
}  // namespace net