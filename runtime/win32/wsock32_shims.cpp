// Winsock (wsock32.dll & ws2_32.dll) Win64 shims
#include "win32/win32.h"
#include "win32/hle.h"
#include "win32/kernel32_state.h"
#include "runtime/log.h"
#include "x86/lifted.h"

#include <cstring>
#include <string>

namespace recomp::win32 {

using namespace recomp::hle;

// Helper to register both name and ordinal
// Ordinal 115: WSAStartup
HLE64_NAMED("wsock32.dll", "#115", wsock_WSAStartup_ord, 2) {
  gaddr wsa = Arg(c, 1);
  if (wsa) {
    std::memset(g_guest_base + wsa, 0, 400);
    Wr16(wsa + 0, 0x0202); // wVersion
    Wr16(wsa + 2, 0x0202); // wHighVersion
    PutStr(wsa + 4, "WinSock 2.0");
    PutStr(wsa + 261, "Running");
    Wr16(wsa + 390, 32767);  // iMaxSockets
    Wr16(wsa + 392, 65467);  // iMaxUdpDg
  }
  return 0; // SUCCESS
}
HLE64_NAMED("wsock32.dll", "WSAStartup", wsock_WSAStartup, 2) {
  return hle_wsock_WSAStartup_ord(c);
}

// Ordinal 116: WSACleanup
HLE64_NAMED("wsock32.dll", "#116", wsock_WSACleanup_ord, 0) { return 0; }
HLE64_NAMED("wsock32.dll", "WSACleanup", wsock_WSACleanup, 0) { return 0; }

// Ordinal 111: WSAGetLastError
HLE64_NAMED("wsock32.dll", "#111", wsock_WSAGetLastError_ord, 0) { return 0; }
HLE64_NAMED("wsock32.dll", "WSAGetLastError", wsock_WSAGetLastError, 0) { return 0; }

// Byte ordering
HLE64_NAMED("wsock32.dll", "#8", wsock_htonl_ord, 1) { return __builtin_bswap32(uint32_t(Arg(c, 0))); }
HLE64_NAMED("wsock32.dll", "htonl", wsock_htonl, 1) { return __builtin_bswap32(uint32_t(Arg(c, 0))); }

HLE64_NAMED("wsock32.dll", "#14", wsock_ntohl_ord, 1) { return __builtin_bswap32(uint32_t(Arg(c, 0))); }
HLE64_NAMED("wsock32.dll", "ntohl", wsock_ntohl, 1) { return __builtin_bswap32(uint32_t(Arg(c, 0))); }

HLE64_NAMED("wsock32.dll", "#9", wsock_htons_ord, 1) { return __builtin_bswap16(uint16_t(Arg(c, 0))); }
HLE64_NAMED("wsock32.dll", "htons", wsock_htons, 1) { return __builtin_bswap16(uint16_t(Arg(c, 0))); }

HLE64_NAMED("wsock32.dll", "#15", wsock_ntohs_ord, 1) { return __builtin_bswap16(uint16_t(Arg(c, 0))); }
HLE64_NAMED("wsock32.dll", "ntohs", wsock_ntohs, 1) { return __builtin_bswap16(uint16_t(Arg(c, 0))); }

// Host info
HLE64_NAMED("wsock32.dll", "#57", wsock_gethostname_ord, 2) {
  gaddr buf = Arg(c, 0);
  size_t len = size_t(Arg(c, 1));
  std::string n = "localhost";
  if (len < n.size() + 1) return uint64_t(-1);
  PutStr(buf, n);
  return 0;
}
HLE64_NAMED("wsock32.dll", "gethostname", wsock_gethostname, 2) {
  return hle_wsock_gethostname_ord(c);
}

HLE64_NAMED("wsock32.dll", "#52", wsock_gethostbyname_ord, 1) {
  static gaddr hostent = 0;
  if (!hostent) {
    hostent = HeapAllocProcess(128, true);
    gaddr name = hostent + 32;
    gaddr addr = hostent + 64;
    gaddr list = hostent + 80;
    PutStr(name, "localhost");
    Wr8(addr + 0, 127);
    Wr8(addr + 3, 1);
    Wr64(list, addr);
    Wr64(list + 8, 0);
    Wr64(hostent + 0, name);     // h_name
    Wr64(hostent + 8, list + 8); // h_aliases
    Wr16(hostent + 16, 2);       // AF_INET
    Wr16(hostent + 18, 4);       // length
    Wr64(hostent + 24, list);    // h_addr_list
  }
  return hostent;
}
HLE64_NAMED("wsock32.dll", "gethostbyname", wsock_gethostbyname, 1) {
  return hle_wsock_gethostbyname_ord(c);
}

HLE64_NAMED("wsock32.dll", "#10", wsock_inet_addr_ord, 1) {
  std::string ip = GStr(Arg(c, 0));
  if (ip == "127.0.0.1" || ip == "localhost") return 0x0100007F; // 127.0.0.1 in network byte order
  return 0xFFFFFFFF;
}
HLE64_NAMED("wsock32.dll", "inet_addr", wsock_inet_addr, 1) {
  return hle_wsock_inet_addr_ord(c);
}

// Sockets stubs
HLE64_NAMED("wsock32.dll", "#23", wsock_socket_ord, 3) { return uint64_t(-1); /* INVALID_SOCKET */ }
HLE64_NAMED("wsock32.dll", "socket", wsock_socket, 3) { return uint64_t(-1); }

HLE64_NAMED("wsock32.dll", "#3", wsock_closesocket_ord, 1) { return 0; }
HLE64_NAMED("wsock32.dll", "closesocket", wsock_closesocket, 1) { return 0; }

HLE64_NAMED("wsock32.dll", "#2", wsock_bind_ord, 3) { return uint64_t(-1); }
HLE64_NAMED("wsock32.dll", "bind", wsock_bind, 3) { return uint64_t(-1); }

HLE64_NAMED("wsock32.dll", "#13", wsock_listen_ord, 2) { return uint64_t(-1); }
HLE64_NAMED("wsock32.dll", "listen", wsock_listen, 2) { return uint64_t(-1); }

HLE64_NAMED("wsock32.dll", "#1", wsock_accept_ord, 3) { return uint64_t(-1); }
HLE64_NAMED("wsock32.dll", "accept", wsock_accept, 3) { return uint64_t(-1); }

HLE64_NAMED("wsock32.dll", "#4", wsock_connect_ord, 3) { return uint64_t(-1); }
HLE64_NAMED("wsock32.dll", "connect", wsock_connect, 3) { return uint64_t(-1); }

HLE64_NAMED("wsock32.dll", "#16", wsock_recv_ord, 4) { return uint64_t(-1); }
HLE64_NAMED("wsock32.dll", "recv", wsock_recv, 4) { return uint64_t(-1); }

HLE64_NAMED("wsock32.dll", "#17", wsock_recvfrom_ord, 6) { return uint64_t(-1); }
HLE64_NAMED("wsock32.dll", "recvfrom", wsock_recvfrom, 6) { return uint64_t(-1); }

HLE64_NAMED("wsock32.dll", "#19", wsock_send_ord, 4) { return uint64_t(-1); }
HLE64_NAMED("wsock32.dll", "send", wsock_send, 4) { return uint64_t(-1); }

HLE64_NAMED("wsock32.dll", "#20", wsock_sendto_ord, 6) { return uint64_t(-1); }
HLE64_NAMED("wsock32.dll", "sendto", wsock_sendto, 6) { return uint64_t(-1); }

HLE64_NAMED("wsock32.dll", "#6", wsock_getsockname_ord, 3) { return uint64_t(-1); }
HLE64_NAMED("wsock32.dll", "getsockname", wsock_getsockname, 3) { return uint64_t(-1); }

HLE64_NAMED("wsock32.dll", "#7", wsock_getsockopt_ord, 5) { return uint64_t(-1); }
HLE64_NAMED("wsock32.dll", "getsockopt", wsock_getsockopt, 5) { return uint64_t(-1); }

HLE64_NAMED("wsock32.dll", "#21", wsock_setsockopt_ord, 5) { return 0; }
HLE64_NAMED("wsock32.dll", "setsockopt", wsock_setsockopt, 5) { return 0; }

HLE64_NAMED("wsock32.dll", "#12", wsock_ioctlsocket_ord, 3) { return uint64_t(-1); }
HLE64_NAMED("wsock32.dll", "ioctlsocket", wsock_ioctlsocket, 3) { return uint64_t(-1); }

HLE64_NAMED("wsock32.dll", "#18", wsock_select_ord, 5) { return 0; }
HLE64_NAMED("wsock32.dll", "select", wsock_select, 5) { return 0; }

// ws2_32.dll
HLE64("ws2_32.dll", getnameinfo, 7) { return uint64_t(-1); }
HLE64_NAMED("ws2_32.dll", "#151", ws2_WSAFDIsSet_ord, 2) { return 0; }
HLE64_NAMED("ws2_32.dll", "__WSAFDIsSet", ws2_WSAFDIsSet, 2) { return 0; }

}  // namespace recomp::win32
