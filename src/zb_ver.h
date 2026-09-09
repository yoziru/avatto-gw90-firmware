#ifndef ZB_VER_H_
#define ZB_VER_H_

#include <stdint.h>

// Decoded CC2652P ZNP version response (11-byte payload).
struct ZbVerInfo
{
  uint8_t transportrev;
  uint8_t product;
  uint8_t majorrel;
  uint8_t minorrel;
  uint8_t maintrel;
  uint32_t zbRev;
};

enum class ZbRole : uint8_t
{
  UNKNOWN,
  ROUTER,
  COORDINATOR
};

constexpr uint8_t ZB_DEVICE_STATE_ROUTER = 7;
constexpr uint8_t ZB_DEVICE_STATE_COORDINATOR = 9;

inline ZbRole zbRoleFromDeviceState(uint8_t deviceState) noexcept
{
  if (deviceState == ZB_DEVICE_STATE_ROUTER)
    return ZbRole::ROUTER;
  if (deviceState == ZB_DEVICE_STATE_COORDINATOR)
    return ZbRole::COORDINATOR;
  return ZbRole::UNKNOWN;
}

inline const char *zbRoleName(ZbRole role) noexcept
{
  switch (role)
  {
  case ZbRole::ROUTER:
    return "Router";
  case ZbRole::COORDINATOR:
    return "Coordinator";
  default:
    return "unknown";
  }
}

// Decode the 11-byte ZNP version payload (as read in getZbVer()).
inline ZbVerInfo decodeZbVer(const uint8_t buf[11]) noexcept
{
  ZbVerInfo v;
  v.transportrev = buf[0];
  v.product = buf[1];
  v.majorrel = buf[2];
  v.minorrel = buf[3];
  v.maintrel = buf[4];
  v.zbRev = buf[5] | (buf[6] << 8) | (buf[7] << 16) | (uint32_t(buf[8]) << 24);
  return v;
}

#endif
