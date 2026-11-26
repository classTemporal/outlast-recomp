#pragma once
// Guest-visible COM objects backed by host C++ objects.
//
// A guest interface pointer points at a small block in guest memory:
//   32-bit:
//     +0 vtable (guest array of 32-bit thunk addresses)
//     +4 magic 'RCOM'
//     +8 object id (host table key)
//   64-bit:
//     +0 vtable (guest array of 64-bit thunk addresses)
//     +8 magic 'RCOM'
//     +16 object id (host table key)
//
// Methods are HLE handlers named "Interface::Method" (in the DLL that
// provides the interface), so an unimplemented method fails by name.

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "win32/process.h"

namespace recomp::guest {
struct GUID {
  uint32_t Data1;
  uint16_t Data2;
  uint16_t Data3;
  uint8_t Data4[8];
};
}

namespace recomp::com {

struct Object {
  virtual ~Object() = default;
  gaddr guest = 0;  // the guest interface pointer (32-bit or 64-bit)
  uint32_t refs = 1;
};

// Builds (once per name) the guest vtable for `iface` with the given
// method names, in vtable order.
gaddr Vtable(const char* dll, const char* iface,
             const std::vector<const char*>& methods);

// Gives `obj` its guest pointer with `vtable`. Returns the pointer.
gaddr Publish(std::shared_ptr<Object> obj, gaddr vtable);

// Host object behind a guest interface pointer (nullptr if not ours).
std::shared_ptr<Object> Lookup(gaddr guest);

template <typename T>
T* As(gaddr guest) {
  auto o = Lookup(guest);
  return dynamic_cast<T*>(o.get());
}

uint32_t AddRef(gaddr guest);
// Drops a reference; destroys the object at zero. Returns the new count.
uint32_t Release(gaddr guest);

bool GuidEq(gaddr guest_guid, const guest::GUID& g);
guest::GUID ReadGuid(gaddr va);

using CreateFn = uint32_t (*)(gaddr iid, gaddr* out);
void RegisterCoClass(const guest::GUID& clsid, CreateFn create);
CreateFn FindCoClass(gaddr clsid);

struct CoClassRegistrar {
  CoClassRegistrar(const guest::GUID& clsid, CreateFn create) {
    RegisterCoClass(clsid, create);
  }
};

gaddr AllocBstr(const std::u16string& s);
void FreeBstr(gaddr bstr);

constexpr uint32_t kS_OK = 0, kE_NOINTERFACE = 0x80004002;

}  // namespace recomp::com
