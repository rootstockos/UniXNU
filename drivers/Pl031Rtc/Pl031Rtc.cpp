// SPDX-License-Identifier: BSD-2-Clause-Patent
//
// ARM PrimeCell PL031 real-time clock ("arm,pl031"), published as XNU's IORTC resource. QEMU's virt
// machine has one at 0x9010000. The counter is 32-bit seconds: RTCDR (0x00) reads it, RTCLR (0x08)
// loads it, and bit 0 of RTCCR (0x0c) starts it (ARM DDI 0224, PrimeCell RTC (PL031) Technical Reference
// Manual). The counter is writable, so the platform sets it directly and keeps no offset.

#include <IOKit/IODeviceMemory.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/rtc/IORTCController.h>

// [0] stage (1 mapped, 2 running, 3 published; negative: the step that failed), [1] peripheral ID,
// [2] last counter, [3] writes.
extern "C" {
volatile int32_t pl031_rtc_stats[4] = {};
}

namespace {
constexpr uint32_t kData = 0x00, kLoad = 0x08, kControl = 0x0c, kControlStart = 1u << 0;
constexpr uint32_t kPeriphId0 = 0xfe0, kPartNumber = 0x031;
}

class Pl031Rtc : public IORTC {
  OSDeclareDefaultStructors (Pl031Rtc)
public:
  bool start (IOService *provider) override;
  long getGMTTimeOfDay (void) override;
  void setGMTTimeOfDay (long secs) override;
  void setAlarmEnable (IOOptionBits message) override;
private:
  IOMemoryMap *map_;
  volatile uint32_t *regs_;
};

OSDefineMetaClassAndStructors (Pl031Rtc, IORTC)

bool Pl031Rtc::start (IOService *provider)
{
  if (!IORTC::start (provider)) {
    return false;
  }
  IODeviceMemory *memory = provider->getDeviceMemoryWithIndex (0);
  map_ = memory != nullptr ? memory->map (kIOMapAnywhere | kIOMapInhibitCache) : nullptr;
  if (map_ == nullptr || map_->getLength () < 0x1000) {
    pl031_rtc_stats[0] = -1;
    return false;
  }
  regs_ = reinterpret_cast<volatile uint32_t *> (map_->getVirtualAddress ());
  pl031_rtc_stats[0] = 1;
  // Part number 0x031 in PeriphID0 and the low nibble of PeriphID1.
  uint32_t part = (regs_[kPeriphId0 / 4] & 0xff) | ((regs_[kPeriphId0 / 4 + 1] & 0xf) << 8);
  pl031_rtc_stats[1] = (int32_t)part;
  if (part != kPartNumber) {
    pl031_rtc_stats[0] = -2;
    return false;
  }
  if (!(regs_[kControl / 4] & kControlStart)) {
    regs_[kControl / 4] = kControlStart;
  }
  pl031_rtc_stats[0] = 2;
  registerService ();
  publishResource ("IORTC", this);
  pl031_rtc_stats[0] = 3;
  IOLog ("Pl031Rtc: counter %u\n", regs_[kData / 4]);
  return true;
}

long Pl031Rtc::getGMTTimeOfDay (void)
{
  uint32_t seconds = regs_ != nullptr ? regs_[kData / 4] : 0;
  pl031_rtc_stats[2] = (int32_t)seconds;
  return (long)seconds;
}

void Pl031Rtc::setGMTTimeOfDay (long secs)
{
  if (regs_ == nullptr || secs <= 0 || secs > 0xffffffffl) {
    return;
  }
  regs_[kLoad / 4] = (uint32_t)secs;
  pl031_rtc_stats[3] = pl031_rtc_stats[3] + 1;
}

void Pl031Rtc::setAlarmEnable (IOOptionBits message)
{
}
