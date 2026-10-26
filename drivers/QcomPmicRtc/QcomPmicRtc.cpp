// SPDX-License-Identifier: BSD-2-Clause-Patent
//
// Real-time clock of Qualcomm SPMI PMICs ("qcom,pm8941-rtc", "qcom,pmk8350-rtc"), the counter mainline
// Linux drives with rtc-pm8xxx, published as XNU's IORTC resource.
//
// Only arm-io children get platform nubs, so the driver matches the SPMI PMIC arbiter
// ("qcom,spmi-pmic-arb") and finds the RTC below one of its PMIC nodes, laid out as Linux describes it:
//   spmi (reg-names "core", "obsrvr", "cnfg"; qcom,ee) > pmic (reg = <SID type>) > rtc (reg = <base>, ...)
// Reads go through this EE's observer channel of the RTC's APID, as spmi-pmic-arb does for arbiter
// versions 5 and 7 (bus 0). Version 8 changed the channel layout and is refused.
// The counter is 32-bit seconds at base + 0x48 (RTC_RDATA0..3); it runs while bit 7 of base + 0x46
// (RTC_CTRL) is set. rtc-pm8xxx uses these offsets for both compatibles.
//
// Phones give the apps processor no write access to the counter; mainline writes it only with
// "allow-set-time". The driver never writes and always publishes IORTCReadOnly, so the platform keeps the
// wall clock as counter + offset.

#include <IOKit/IODeviceMemory.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOLocks.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/rtc/IORTCController.h>
#include <libkern/c++/OSBoolean.h>
#include <libkern/c++/OSData.h>
#include <libkern/c++/OSIterator.h>

// [0] stage (1 RTC node found, 2 arbiter mapped, 3 APID found, 4 counter running, 5 published; negative:
// the step that failed), [1] arbiter version, [2] APID, [3] last counter, [4] reads, [5] failed reads,
// [6] last arbiter status.
extern "C" {
volatile int32_t qcom_pmic_rtc_stats[7] = {};
}

namespace {
constexpr uint32_t kVersion5 = 0x50000000, kVersion7 = 0x70000000, kVersion8 = 0x80000000;
constexpr uint32_t kFeatures = 0x04, kFeaturesApids = 0x7ff;
constexpr uint32_t kCmd = 0x00, kStatus = 0x08, kRdata0 = 0x18;
constexpr uint32_t kOpExtReadLong = 1;
constexpr uint32_t kStatusDone = 1u << 0, kStatusErrors = 0xeu;   // failed, denied, dropped
constexpr uint8_t kRtcCtrl = 0x46, kRtcRead = 0x48, kRtcEnable = 1u << 7;

struct Layout {
  uint32_t apidMap, owner, eeStride, apidStride, maxApids;
};
constexpr Layout kLayout5 = {0x900, 0x700, 0x10000, 0x80, 512};
constexpr Layout kLayout7 = {0x2000, 0x000, 0x8000, 0x20, 1024};

bool isCompatible (IORegistryEntry *entry, const char *const *names)
{
  OSData *compatible = OSDynamicCast (OSData, entry->getProperty ("compatible"));
  if (compatible == nullptr) {
    return false;
  }
  const char *cursor = static_cast<const char *> (compatible->getBytesNoCopy ());
  const char *end = cursor + compatible->getLength ();
  while (cursor < end) {
    size_t length = strnlen (cursor, (size_t)(end - cursor));
    for (const char *const *name = names; *name != nullptr; ++name) {
      if (length == strlen (*name) && strncmp (cursor, *name, length) == 0) {
        return true;
      }
    }
    cursor += length + 1;
  }
  return false;
}

bool firstCell (IORegistryEntry *entry, uint32_t *value)
{
  OSData *reg = OSDynamicCast (OSData, entry->getProperty ("reg"));
  if (reg == nullptr || reg->getLength () < sizeof (uint32_t)) {
    return false;
  }
  memcpy (value, reg->getBytesNoCopy (), sizeof (uint32_t));
  return true;
}

// Index of a name in the provider's "reg-names", which orders its device memory.
int registerIndex (IOService *provider, const char *name)
{
  OSData *names = OSDynamicCast (OSData, provider->getProperty ("reg-names"));
  if (names == nullptr) {
    return -1;
  }
  const char *cursor = static_cast<const char *> (names->getBytesNoCopy ());
  const char *end = cursor + names->getLength ();
  for (int index = 0; cursor < end; ++index) {
    size_t length = strnlen (cursor, (size_t)(end - cursor));
    if (length == strlen (name) && strncmp (cursor, name, length) == 0) {
      return index;
    }
    cursor += length + 1;
  }
  return -1;
}
}

class QcomPmicRtc : public IORTC {
  OSDeclareDefaultStructors (QcomPmicRtc)
public:
  bool start (IOService *provider) override;
  long getGMTTimeOfDay (void) override;
  void setGMTTimeOfDay (long secs) override;
  void setAlarmEnable (IOOptionBits message) override;
private:
  IOMemoryMap *core_;
  IOMemoryMap *cnfg_;
  IOMemoryMap *obsrvr_;
  volatile uint32_t *channel_;
  uint8_t base_;
  IOLock *lock_;

  bool locate (IOService *provider, uint32_t *sid, uint32_t *base);
  IOMemoryMap *mapNamed (IOService *provider, const char *name);
  int findApid (uint32_t ppid, uint32_t ee, const Layout &layout);
  int read (uint8_t reg, uint8_t count, uint32_t *value);
  bool counter (uint32_t *seconds);
  bool fail (int step);
};

OSDefineMetaClassAndStructors (QcomPmicRtc, IORTC)

bool QcomPmicRtc::fail (int step)
{
  qcom_pmic_rtc_stats[0] = -step;
  IOLog ("QcomPmicRtc: step %d failed\n", step);
  return false;
}

// The first RTC below the arbiter's PMIC nodes: its PMIC's SID and its register base.
bool QcomPmicRtc::locate (IOService *provider, uint32_t *sid, uint32_t *base)
{
  static const char *const rtcNames[] = {"qcom,pmk8350-rtc", "qcom,pm8941-rtc", nullptr};
  bool found = false;
  OSIterator *pmics = provider->getChildIterator (gIODTPlane);
  if (pmics == nullptr) {
    return false;
  }
  IORegistryEntry *pmic;
  while (!found && (pmic = OSDynamicCast (IORegistryEntry, pmics->getNextObject ())) != nullptr) {
    uint32_t pmicSid;
    if (!firstCell (pmic, &pmicSid)) {
      continue;
    }
    OSIterator *children = pmic->getChildIterator (gIODTPlane);
    if (children == nullptr) {
      continue;
    }
    IORegistryEntry *child;
    while (!found && (child = OSDynamicCast (IORegistryEntry, children->getNextObject ())) != nullptr) {
      uint32_t rtcBase;
      if (isCompatible (child, rtcNames) && firstCell (child, &rtcBase)) {
        *sid = pmicSid;
        *base = rtcBase;
        found = true;
      }
    }
    children->release ();
  }
  pmics->release ();
  return found;
}

IOMemoryMap *QcomPmicRtc::mapNamed (IOService *provider, const char *name)
{
  int index = registerIndex (provider, name);
  IODeviceMemory *memory = index >= 0 ? provider->getDeviceMemoryWithIndex ((unsigned)index) : nullptr;
  return memory != nullptr ? memory->map (kIOMapAnywhere | kIOMapInhibitCache) : nullptr;
}

// The APID that serves the PPID: the first mapping, replaced by one this EE owns, as spmi-pmic-arb picks it.
int QcomPmicRtc::findApid (uint32_t ppid, uint32_t ee, const Layout &layout)
{
  volatile uint32_t *core = reinterpret_cast<volatile uint32_t *> (core_->getVirtualAddress ());
  volatile uint32_t *cnfg = reinterpret_cast<volatile uint32_t *> (cnfg_->getVirtualAddress ());
  uint32_t apids = core[kFeatures / 4] & kFeaturesApids;
  if (apids > layout.maxApids) {
    apids = layout.maxApids;
  }
  int found = -1;
  for (uint32_t apid = 0; apid < apids; ++apid) {
    uint32_t offset = layout.apidMap + 4 * apid;
    if (offset + 4 > core_->getLength () || layout.owner + 4 * apid + 4 > cnfg_->getLength ()) {
      break;
    }
    uint32_t map = core[offset / 4];
    if (map == 0 || ((map >> 8) & 0xfff) != ppid) {
      continue;
    }
    uint32_t owner = cnfg[(layout.owner + 4 * apid) / 4] & 7u;
    if (found < 0 || owner == ee) {
      found = (int)apid;
    }
  }
  return found;
}

// count bytes (1..4) from base + reg, little-endian. 0, or the failing arbiter status (-1 on timeout).
int QcomPmicRtc::read (uint8_t reg, uint8_t count, uint32_t *value)
{
  IOLockLock (lock_);
  channel_[kCmd / 4] = (kOpExtReadLong << 27) | ((uint32_t)(uint8_t)(base_ + reg) << 4) | ((count - 1u) & 7u);
  int result = -1;
  for (unsigned poll = 0; poll < 1000; ++poll) {
    uint32_t status = channel_[kStatus / 4];
    if (status & kStatusDone) {
      result = (status & kStatusErrors) ? (int)status : 0;
      break;
    }
    IODelay (1);
  }
  if (result == 0) {
    uint32_t data = channel_[kRdata0 / 4];
    *value = count >= 4 ? data : data & ((1u << (8 * count)) - 1u);
  }
  IOLockUnlock (lock_);
  if (result != 0) {
    qcom_pmic_rtc_stats[5] = qcom_pmic_rtc_stats[5] + 1;
    qcom_pmic_rtc_stats[6] = result;
  }
  return result;
}

// The counter, read as rtc-pm8xxx reads it: all four bytes, then the low byte again; a smaller low byte
// means a carry between the two reads, so the four bytes are read once more.
bool QcomPmicRtc::counter (uint32_t *seconds)
{
  uint32_t value = 0, low = 0;
  if (read (kRtcRead, 4, &value) != 0 || read (kRtcRead, 1, &low) != 0) {
    return false;
  }
  if (low < (value & 0xff) && read (kRtcRead, 4, &value) != 0) {
    return false;
  }
  qcom_pmic_rtc_stats[3] = (int32_t)value;
  qcom_pmic_rtc_stats[4] = qcom_pmic_rtc_stats[4] + 1;
  *seconds = value;
  return true;
}

bool QcomPmicRtc::start (IOService *provider)
{
  if (!IORTC::start (provider)) {
    return false;
  }
  uint32_t sid = 0, base = 0;
  if (!locate (provider, &sid, &base) || sid > 0xf || base > 0xffff) {
    return fail (1);
  }
  qcom_pmic_rtc_stats[0] = 1;
  uint32_t ee = 0;
  if (OSData *value = OSDynamicCast (OSData, provider->getProperty ("qcom,ee"))) {
    if (value->getLength () == sizeof (uint32_t)) {
      memcpy (&ee, value->getBytesNoCopy (), sizeof (ee));
    }
  }
  lock_ = IOLockAlloc ();
  core_ = mapNamed (provider, "core");
  cnfg_ = mapNamed (provider, "cnfg");
  obsrvr_ = mapNamed (provider, "obsrvr");
  if (lock_ == nullptr || core_ == nullptr || cnfg_ == nullptr || obsrvr_ == nullptr || ee > 7) {
    return fail (2);
  }
  uint32_t version = reinterpret_cast<volatile uint32_t *> (core_->getVirtualAddress ())[0];
  qcom_pmic_rtc_stats[1] = (int32_t)version;
  if (version < kVersion5 || version >= kVersion8) {
    return fail (3);
  }
  const Layout &layout = version >= kVersion7 ? kLayout7 : kLayout5;
  qcom_pmic_rtc_stats[0] = 2;
  int apid = findApid ((sid << 8) | (base >> 8), ee, layout);
  qcom_pmic_rtc_stats[2] = apid;
  uint64_t channel = (uint64_t)layout.eeStride * ee + (uint64_t)layout.apidStride * (uint32_t)apid;
  if (apid < 0 || channel + layout.apidStride > obsrvr_->getLength ()) {
    return fail (4);
  }
  channel_ = reinterpret_cast<volatile uint32_t *> (obsrvr_->getVirtualAddress () + channel);
  base_ = (uint8_t)(base & 0xff);
  qcom_pmic_rtc_stats[0] = 3;
  uint32_t control = 0, seconds = 0;
  if (read (kRtcCtrl, 1, &control) != 0 || !(control & kRtcEnable) || !counter (&seconds)) {
    return fail (5);
  }
  qcom_pmic_rtc_stats[0] = 4;
  setProperty ("IORTCReadOnly", kOSBooleanTrue);
  registerService ();
  publishResource ("IORTC", this);
  qcom_pmic_rtc_stats[0] = 5;
  IOLog ("QcomPmicRtc: SID %u base 0x%x APID %d, arbiter 0x%08x, counter %u\n", sid, base, apid, version, seconds);
  return true;
}

long QcomPmicRtc::getGMTTimeOfDay (void)
{
  uint32_t seconds = 0;
  return counter (&seconds) ? (long)seconds : 0;
}

void QcomPmicRtc::setGMTTimeOfDay (long secs)
{
}

void QcomPmicRtc::setAlarmEnable (IOOptionBits message)
{
}
