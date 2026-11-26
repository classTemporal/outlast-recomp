// shell32.dll and advapi32.dll Win64 HLE shims for Outlast
#include "win32/win32.h"
#include "win32/hle.h"
#include "win32/kernel32_state.h"
#include "runtime/log.h"
#include "x86/lifted.h"

#include <cstring>
#include <string>

namespace recomp::win32 {

using namespace recomp::hle;

// CSIDL directory resolution
static std::string FolderFor(uint32_t csidl) {
  const std::string user = "C:\\Users\\Player";
  switch (csidl & 0xFF) {
    case 0x05: return user + "\\Documents";                 // CSIDL_PERSONAL / My Documents
    case 0x1A: return user + "\\AppData\\Roaming";          // CSIDL_APPDATA
    case 0x1C: return user + "\\AppData\\Local";            // CSIDL_LOCAL_APPDATA
    case 0x23: return "C:\\ProgramData";                    // CSIDL_COMMON_APPDATA
    case 0x2E: return "C:\\Users\\Public\\Documents";       // CSIDL_COMMON_DOCUMENTS
    case 0x26: return "C:\\Program Files";
    case 0x24: return "C:\\WINDOWS";
    case 0x25: return "C:\\WINDOWS\\system32";
    case 0x10: return user + "\\Desktop";
    default: return user + "\\Documents";
  }
}

// shell32.dll SHGetFolderPathW(hwnd, csidl, hToken, dwFlags, pszPath)
HLE64("shell32.dll", SHGetFolderPathW, 5) {
  uint32_t csidl = uint32_t(Arg(c, 1));
  gaddr out = Arg(c, 4);
  std::string p = FolderFor(csidl);
  MakeDirs(HostPath(p, PathAccess::kWrite));
  if (out) PutWStr(out, p);
  return 0; // S_OK
}

// shell32.dll ShellExecuteW
HLE64("shell32.dll", ShellExecuteW, 6) {
  return 42; // HINSTANCE > 32 indicates success
}

// advapi32.dll registry shims (Outlast checks display / sound settings)
HLE64("advapi32.dll", RegOpenKeyExA, 5) {
  gaddr phkResult = Arg(c, 4);
  if (phkResult) Wr64(phkResult, 0x1000); // Dummy HKEY
  return 0; // ERROR_SUCCESS
}

HLE64("advapi32.dll", RegOpenKeyExW, 5) {
  gaddr phkResult = Arg(c, 4);
  if (phkResult) Wr64(phkResult, 0x1000);
  return 0;
}

HLE64("advapi32.dll", RegCloseKey, 1) {
  return 0;
}

HLE64("advapi32.dll", RegQueryValueExW, 6) {
  gaddr lpType = Arg(c, 3);
  gaddr lpData = Arg(c, 4);
  gaddr lpcbData = Arg(c, 5);
  // Return key not found for generic queries so Outlast uses its game configs
  return 2; // ERROR_FILE_NOT_FOUND
}

HLE64("advapi32.dll", RegQueryValueExA, 6) {
  return 2; // ERROR_FILE_NOT_FOUND
}

HLE64("advapi32.dll", RegEnumKeyExA, 8) {
  return 259; // ERROR_NO_MORE_ITEMS
}

HLE64("advapi32.dll", RegQueryInfoKeyW, 12) {
  return 0;
}

HLE64("advapi32.dll", GetUserNameW, 2) {
  gaddr buf = Arg(c, 0);
  gaddr psize = Arg(c, 1);
  std::string name = "Player";
  if (buf && psize) {
    uint32_t sz = Rd32(psize);
    if (sz < name.size() + 1) {
      Wr32(psize, uint32_t(name.size() + 1));
      return 0;
    }
    PutWStr(buf, name);
    Wr32(psize, uint32_t(name.size() + 1));
  }
  return 1;
}

}  // namespace recomp::win32
