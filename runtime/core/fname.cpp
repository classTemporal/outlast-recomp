#include "runtime/fname.h"

#include <cstdio>

#include "runtime/core/env.h"
#include "runtime/core/log.h"

namespace recomp::fname {

bool Table(const GuestMemory& mem, uint64_t* data, uint32_t* num) {
  *data = mem.read<uint64_t>(kGNamesData);
  *num = mem.read<uint32_t>(kGNamesNum);
  // A count this large is not a real GNames table (the guest's is ~1M
  // entries); it means the field has not been written yet.
  if (*num > 0x1000000u) return false;
  return *data != 0 && *num != 0;
}

void DumpLookup(const GuestMemory& mem, const char* where, int32_t index,
                int32_t number) {
  uint64_t data = 0;
  uint32_t num = 0;
  const bool ok = Table(mem, &data, &num);
  if (!ok) {
    RECOMP_TRACE("  FName %s: index=%d number=%d (GNames not ready: data=%llX num=%u)\n",
                 where, index, number, (unsigned long long)data, num);
    return;
  }
  if (index < 0 || uint32_t(index) >= num) {
    RECOMP_TRACE("  FName %s: index=%d number=%d OUT OF RANGE (GNames.Num=%u)\n",
                 where, index, number, num);
    return;
  }
  const uint64_t entry = mem.read<uint64_t>(data + uint64_t(index) * 8);
  RECOMP_TRACE("  FName %s: index=%d number=%d -> entry %llX\n", where, index, number,
               (unsigned long long)entry);
}

bool Strict() {
  static const bool strict = [] {
    const char* e = Env("FNAME_STRICT");
    return e && e[0] == '1';
  }();
  return strict;
}

}  // namespace recomp::fname
