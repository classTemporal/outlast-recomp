#pragma once
// FName diagnostics and the "no symptom patches" switch.
//
// UE3 names every object, property and package with an FName: a pair
// {index, number} into the GNames table. When an index does not resolve the
// engine formats the sentinel, which is what produces "<uninitialized>" and
// "None" in error messages. The lifted code carries hand-written guards that
// force such an index to 0 and continue; they keep the boot alive but hide
// which name was being built. This header is the runtime side of seeing it.

#include <cstdint>

#include "runtime/guest_memory.h"

namespace recomp::fname {

// GNames' data pointer and element count, as the guest sees them.
constexpr gaddr kGNamesData = 0x141FF8FA0ull;
constexpr gaddr kGNamesNum = 0x141FF8FA8ull;

// Reads the table. Returns false when it is not usable (not allocated yet,
// or a count that cannot be right), in which case nothing is logged.
bool Table(const GuestMemory& mem, uint64_t* data, uint32_t* num);

// Logs one FName resolution attempt: the pair, the table state, and the
// entry's first bytes when the index is in range. `where` names the lifted
// function doing the lookup (f_<va>).
void DumpLookup(const GuestMemory& mem, const char* where, int32_t index,
                int32_t number);

// True when the caller should keep the original behaviour (clamp and
// continue). Set by <ID>_FNAME_STRICT=0 (the default): the guards stay, so a
// boot is never made worse by diagnostics. With =1 the caller instead skips
// the clamp, which surfaces the bad index as the fault it is.
bool Strict();

}  // namespace recomp::fname
