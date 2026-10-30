# Redmi Note 12 Turbo / POCO F5 \(marble\), Qualcomm SM7475 \(Snapdragon 7+ Gen 2\) \(MiLacy\)

[MiLacy/UniXNU-marble](https://github.com/MiLacy/UniXNU-marble)

<div v-pre>

Format-2 first-console port for the Redmi Note 12 Turbo / POCO F5 \(SM7475\). The UEFI image and UniXNU menu are buildable; XNU kernel entry needs user-supplied IPSW inputs; system entry is a documented later stage.

Source record: [porting.toml](https://github.com/MiLacy/UniXNU-marble/blob/7f9a3e506235bf69479faf8f47dcda663fa9e658/porting.toml), `7f9a3e506235bf69479faf8f47dcda663fa9e658`.

This page is generated from the device record.

| Area | Status |
| --- | --- |
| [Configuration](#configuration) | `implemented` |
| [Firmware and boot](#firmware) | `host-verified` |
| [CPU and interrupts](#cpu) | `planned` |
| [Memory and DMA](#memory) | `planned` |
| [Storage and root](#storage) | `planned` |
| [Display](#display) | `planned` |
| [Input](#input) | `planned` |
| [GPU](#gpu) | `not-applicable` |
| [Network](#network) | `not-applicable` |
| [Audio](#audio) | `not-applicable` |
| [Power](#power) | `planned` |
| [Image assembly](#packaging) | `host-verified` |
| [Diagnostics](#diagnostics) | `planned` |

## Configuration {#configuration}

`implemented`

### Hardware and dependencies

- Board identity, SM7475 SoC profile, iPadOS 17.7.11 OS profile, component selection and external source pins.

### Configuration

- board.toml selects soc\_source=sm7475, kit=marble and the iPadOS 17.7.11 profile. The SM7475 DTSI and SMP/cluster patches are pinned through feeds.toml from the sibling UniXNU-sm7475-common repository.

### Procedure

- Run ./unixnu defconfig marble\_defconfig, fetch, status and tree from the main UniXNU checkout; inspect the resolved sources and output path.

### Development findings

- The common ADT generator now emits AAPL,phandle instead of the FDT spelling phandle. XNU's AddPHandle/IODTFindInterruptParent look up AAPL,phandle; without the rename the UFS interrupt-parent resolved to NULL and IODTGetICellCounts\(\) data-aborted at PC 0xfffffff0078ec2d4 \(ESR 0x96000005, FAR 0\).
- The board is SM7475-AB \(Cape\), not SM8550. The CPU topology is 4x A510, 3x A710 and 1x X2, so the shared DTSI differs from SM8550 even though GIC and UFS addresses match.
- The apps-SMMU stream from the vendor DT is 0xE0, while upstream ShengUfs programs 0x60. The local drivers/ShengUfs fork adds native-ufs-stream with a 0xE0 default.
- The stock marble UEFI port provides Resources/DTBs/marble.dtb. The port adds Resources/DTBs/marble-unixnu.dtb with the stale /chosen initrd properties removed, keeping the splash region, so the bootloader can publish the actual payload range and the board does not add a duplicate \[\[bootimg.dtb\_node\]\].

### Completion criteria

- All source pins resolve; status shows the intended SoC, OS and device layers; the device tree compiles.

### Current limits

- The SM7475 shared repository is published at https://github.com/MiLacy/UniXNU-sm7475-common and pinned to fc78deda27304248186fec2320c5678c45d8e431.

### Next action

- Run the status check on a clean clone of the published repositories.

### Evidence

- `source`: [Board identity, component selection, firmware paths and payload inputs.](https://github.com/MiLacy/UniXNU-marble/blob/7f9a3e506235bf69479faf8f47dcda663fa9e658/board.toml)
- `source`: [Board memory, splash reservation and CPU cluster tags.](https://github.com/MiLacy/UniXNU-marble/blob/7f9a3e506235bf69479faf8f47dcda663fa9e658/dts/marble.dts)
- `source`: [First-console kernel configuration.](https://github.com/MiLacy/UniXNU-marble/blob/7f9a3e506235bf69479faf8f47dcda663fa9e658/configs/marble_defconfig)
- `source`: [Source-backed hardware inventory.](https://github.com/MiLacy/UniXNU-marble/blob/7f9a3e506235bf69479faf8f47dcda663fa9e658/docs/HARDWARE.md)

## Firmware and boot {#firmware}

`host-verified`

Components: `DarwinBoot`.

### Hardware and dependencies

- Mu-Silicium-marble Platforms/Xiaomi/marblePkg, Resources/Configs/marble.toml, the stock Resources/DTBs/marble.dtb and the derived Resources/DTBs/marble-unixnu.dtb, and the four Mu edk2 patches.

### Configuration

- \[firmware\] selects config=marble, the marble DSC, the 3 MiB FD path and the four Mu patches. UniXNU, UniXNUDxe, UniXNURamDiskDxe and DarwinBoot are listed in the DSC and DXE.inc.

### Procedure

- Build the firmware with ./unixnu build firmware, then pack an empty-ramdisk boot.img with the marble DTB and boot it with fastboot boot.

### Development findings

- The stock marble DSC already includes DiskIoDxe, PartitionDxe, Fat and EnglishDxe; the UniXNU integration adds the four modules and the UniXNULib, FdtLib and HobLib mappings.
- The FD is 3 MiB at 0xA7100000, the same window the stock Mu-Silicium marble port uses.
- Host build 2026-10-30: the DEBUG FD is exactly 0x300000 bytes \(SHA-256 6c52e8a4dcdd1483d7ca672de67725f8c76a60ececa956ba81d38a3aa4f7d4b4\), and the build log contains UniXNU.efi, UniXNUDxe.efi, UniXNURamDiskDxe.efi and DarwinBoot.efi.
- The stock BootShim header was updated to describe the complete shim plus payload; without that fix the UniXNU boot-image check rejects the image.
- The marble SEC did not save the previous stage's x0 or build gSiliciumEntryArgHobGuid, so UniXNURamDiskDxe could not publish the payload volume and the menu reported no boot entry found. ModuleEntryPoint.S now keeps x0 in x19 and passes it as the third CEntryPoint argument; Sec.c builds the HOB after InitializeMemory. The GUID is present in the FD. DarwinBoot also detects EL2 and returns to EL1h \(SPSR\_EL2/ELR\_EL2 eret, HCR\_EL2.RW=1, CNTHCTL\_EL2=3, CNTVOFF\_EL2=0, CPTR\_EL2=0, CPACR\_EL1 FPEN\) as a defensive guard. The observed fiq\_context\_init panic was already at EL1 and persisted with that guard; the actual cause is Gunyah trapping the EL1 \`CNTP\_CTL\_EL0\` write as UNDEF \(instr d51be228, PC 0xfffffff0074204fc\), fixed by the device kernel patch marble-virtual-timer-only.patch.
- Resources/DTBs/marble-unixnu.dtb removes the stock /chosen linux,initrd-start/end values so the bootloader's real payload range is the only one the driver sees.

### Completion criteria

- Firmware volume builds at the configured size and contains UniXNU, UniXNUDxe, UniXNURamDiskDxe and DarwinBoot; the menu appears on the phone.

### Current limits

- No hardware boot has been observed in this workspace. The FD size may need RELEASE instead of DEBUG if the added modules overflow 3 MiB.

### Next action

- Run the Stage 1 real-device test with boot-empty.img and report the first screen/log line.

### Evidence

- `host`: [DEBUG FD built at 0x300000 bytes; build log lists UniXNU.efi, UniXNUDxe.efi, UniXNURamDiskDxe.efi and DarwinBoot.efi; the SEC entry-argument HOB GUID is present in the FD.](https://github.com/MiLacy/UniXNU-marble/blob/7f9a3e506235bf69479faf8f47dcda663fa9e658/README.md)
  Command: <code>./unixnu build firmware -D ../UniXNU-marble --uefi ../Mu-Silicium-marble --edk2 ../Mu-Silicium-marble/Mu_Basecore</code>. Revision: `f0721e9745c45ad2600cd69c74730f1cca55d56a`.

## CPU and interrupts {#cpu}

`planned`

Components: `ArmDTPlatform`, `ArmCpu`.

### Hardware and dependencies

- Eight CPU affinities, PSCI smc, GIC distributor 0x17100000/0x10000, redistributors 0x17180000/0x200000, stride 0x40000, 19.2 MHz timer.

### Configuration

- sm7475.dtsi declares cpu0-7, the GIC and the PSCI node. marble.dts tags cpu0-3 E and cpu4-7 P. The SoC patch series adds MAX\_CPUS=8, MAX\_CPU\_CLUSTERS=2 and the Edge scheduler policy.

### Procedure

- Apply the SoC patch series, build the kernel and payload, boot the phone and read the ArmCpu SMP counters and per-CPU work rows.

### Development findings

- The vendored stock DT reports Affinity1 core numbers 0x000 through 0x700, so the native-smp-secondary patch is required for secondary CPUs.
- The UFS interrupt specifier now resolves to the GIC: the ADT GIC node carries AAPL,phandle=1 and ufshc interrupt-parent=1. IODTMapInterruptsSharing no longer calls IODTGetICellCounts\(NULL\).
- Gunyah traps the EL1 CNTP\_CTL\_EL0 write as UNDEF \(device witness PC 0xfffffff0074204fc, instruction d51be228, @sleh.c:1045\). marble-virtual-timer-only.patch keeps CNTV enabled and removes the CNTP write, matching the legacy marble port. The kernel now passes fiq\_context\_init.
- The E/P flags must not be enabled without the AMP patch; MAX\_CPU\_CLUSTERS would remain 1 and the topology parser would write past its array.

### Completion criteria

- Every enabled CPU reaches its entry checkpoint; timer interrupts and IPIs work on each core; the P cluster carries the UI workload.

### Current limits

- No real-device SMP result has been recorded. The first-console command line currently includes cpus=8 but not sheng-smp=1.

### Next action

- Boot the first-console kernel, then enable sheng-smp=1, armcpu-spi-ipi=1 and armcpu-self-ipi=1 and record per-core counters.

### Evidence

- `source`: [CPU, GIC and timer values with their vendor-DT source.](https://github.com/MiLacy/UniXNU-marble/blob/7f9a3e506235bf69479faf8f47dcda663fa9e658/docs/HARDWARE.md)
- `source`: [E/P cluster tags and enabled UFS nodes.](https://github.com/MiLacy/UniXNU-marble/blob/7f9a3e506235bf69479faf8f47dcda663fa9e658/dts/marble.dts)

## Memory and DMA {#memory}

`planned`

### Hardware and dependencies

- UEFI memory map, stock DDR banks, PIL/display/TZ reservations and the firmware FD window at 0xA7100000.

### Configuration

- marble.dts declares a 2 GiB handoff memory node at 0x80000000 and the 0x2B00000 splash reservation at 0xB8000000. payload.adt requests a 4 GiB owned window.

### Procedure

- Compare the DTS handoff, the UEFI memory map and the DarwinBoot free-memory runs; adjust owned\_memory\_size only after a real-device NO WINDOW or memory-pressure result.

### Development findings

- The stock 12 GiB DTB has six DDR banks, but only the low region is a contiguous 2 GiB handoff. The owned window is separate and is placed by DarwinBoot in free memory.
- The memory map reserves 0x1C200000 bytes of PIL, 0x2B00000 display/Demura, 0x10000000 kernel, 0x5000000 OEM VM and 0x20000000 MTE space below 4 GiB.

### Completion criteria

- Memory reservations reconcile with the actual UEFI map; every enabled DMA engine stays inside a mapped range.

### Current limits

- No device memory reading exists; owned\_memory\_size is a conservative 4 GiB choice, not a measured value.

### Next action

- Read the DarwinBoot window and the kernel RAM/free/available counters on the phone before increasing the window.

### Evidence

- `source`: [Memory map and reservation table.](https://github.com/MiLacy/UniXNU-marble/blob/7f9a3e506235bf69479faf8f47dcda663fa9e658/docs/HARDWARE.md)

## Storage and root {#storage}

`planned`

Components: `SoftwareAES`, `ShengUfs`, `VirtioMmioBlock`, `RamBlock`, `VerifiedRoot`, `AppleKeyStoreState`.

### Hardware and dependencies

- UFS host 0x1d84000/0x3000, PHY 0x1d80000/0x2000, apps SMMU 0x15000000, UFS stream 0xE0, GCC 0x100000, userdata GPT.

### Configuration

- The local ShengUfs fork adds native-ufs-stream with a 0xE0 default. marble\_defconfig keeps CONFIG\_XNU\_NATIVE\_UFS, the verified-root chain and the Data chain off; the component is selected only when those inputs exist.

### Procedure

- Build the UFS component, boot with native-ufs-root=1, read the capacity/GPT/known blocks, then enable the verified-root bundle and Data seed.

### Development findings

- Upstream ShengUfs hardcodes stream 0x60; the vendor marble DT uses 0xE0. The fork changes the stream argument and the SMR match scan.
- Device probe 2026-10-31 \(read-only, no writes\): UFS host/GCC and the SMMU bank-2 takeover work. userdata first=0x2f6a00 last=0x3b037fa \(4096-byte LBAs, 224.2 GiB\); block0 has no SHNGROOT manifest \(manifest status=1, windows=0\), so it is the Android ext4 userdata. The geometry is now in board.toml \[userdata\].
- The first UFS probe reached SMMU planning and stopped at \`conflicting UFS stream mapping\`. The device reported slot=2 bank=74 smr=800000e0 s2cr=2 id2=5111: the firmware leaves an exclusive SMR for stream 0xE0 mapped to translating context bank 2.
- The fork now accepts that exclusive bank with native-ufs-smmu-takeover=1 and clears SCTLR/CBAR before using it as the driver bypass context. The first takeover build still failed because planUfsSmmu only scanned the top eight banks; the retry range is now the full context-bank count, so it reaches firmware bank 2. The rebuilt probe should continue to the GPT/userdata read.
- The probe build selects ShengUfs with NATIVE\_UFS\_PROBE\_ONLY=1 and passes native-ufs-probe=10. The kernel contains NativeUfs but not NativeUfsBlock/IOBlockStorageDevice, avoiding the unbound storage base symbols; the probe reads the userdata GPT and first block and panics with the geometry.
- Next: fill \[userdata\] first\_lba/last\_lba from the probe, build the OS/SystemOS userdata image, then enable the verified-root/Data/display chain.
- The controller, GCC and SMMU bases match the SM8550 layout, but the per-controller clock/power state and the UFS PHY ownership are not measured on marble.
- ShengUfs's root layout reads the userdata partition from LUN 0's 4096-byte GPT; no geometry is filled in yet.
- A probe-only attempt exposed an upstream compile error: timerTicks\(\) and native\_ufs\_read\_ticks were inside the NATIVE\_VERIFIED\_BLOCK guard. The fork moves them outside it so the transport can be compiled without the verified-root bundle.

### Completion criteria

- UFS registers read, GPT and capacity match, root mounts and launchd starts, Data writes stay inside the measured window.

### Current limits

- No UFS transaction has been run on marble in this workspace. The verified-root bundle, Data seed and root UUID inputs are not provisioned, so UFS is not selected in the first-console build.

### Next action

- After the console boot, capture the GPT, build the verified-root bundle, then select ShengUfs and enable native-ufs-root=1.

### Evidence

- `source`: [Stream override implementation and SM8550 register constants.](https://github.com/MiLacy/UniXNU-marble/blob/7f9a3e506235bf69479faf8f47dcda663fa9e658/drivers/ShengUfs/NativeUfs.cpp)
- `source`: [UFS, SMMU and GCC sources.](https://github.com/MiLacy/UniXNU-marble/blob/7f9a3e506235bf69479faf8f47dcda663fa9e658/docs/HARDWARE.md)

## Display {#display}

`planned`

Components: `GopFramebuffer`.

### Hardware and dependencies

- 1080x2400 GOP framebuffer, splash region 0xB8000000/0x2B00000, UEFI SimpleFbDxe.

### Configuration

- GopFramebuffer is selected but CONFIG\_XNU\_NATIVE\_LEGACY\_DISPLAY is off in the first-console defconfig. The legacy archive and board probe are described in docs/FULL-SYSTEM.md.

### Procedure

- Enable the legacy display archive, add offline-legacy-register=1 and offline-legacy-gop=1, then verify geometry, colors and the userland transition.

### Development findings

- The stock Mu-Silicium marble platform publishes a 1080x2400 32-bit GOP on the ABL framebuffer.
- The sheng/alioth display stack binds the original IOMobileFramebufferLegacy; it is OS-version-specific and must be re-proved against this kernel at build time.

### Completion criteria

- A known frame appears with correct geometry and colors, then the boot progress and Hello screen present.

### Current limits

- The legacy display archive is not wired into this repository yet; the first console relies on the XNU framebuffer console.

### Next action

- Port the display-only parts of the alioth legacy archive, then test the panel on the phone.

### Evidence

- `source`: [Panel and splash values.](https://github.com/MiLacy/UniXNU-marble/blob/7f9a3e506235bf69479faf8f47dcda663fa9e658/docs/HARDWARE.md)

## Input {#input}

`planned`

Components: `QcomGeniSpi`.

### Hardware and dependencies

- Goodix GT9916S on SPI 0x990000, SPI 605, IRQ GPIO21, reset GPIO20, GPI 0x900000, clocks 94/134/135.

### Configuration

- QcomGeniSpi is selected as the controller component. No Goodix protocol or IOHIDDevice publisher is enabled.

### Procedure

- Implement the GENI SPI + GPI DMA + Goodix protocol path, publish IOHID digitizer events and verify contacts and release events.

### Development findings

- The vendor DT gives a complete wiring description, but the GPI/SMMU mapping and the Goodix firmware/config images are not implemented.
- The legacy port reached register reads and a clock snapshot but no physical touch.

### Completion criteria

- Touches map to the display, simultaneous contacts keep stable IDs, release events arrive and Setup is usable.

### Current limits

- No touch driver or firmware image is present. Setup cannot be completed with touch on this image.

### Next action

- Build a minimal Goodix probe that reads the product ID and one touch event before integrating HID.

### Evidence

- `source`: [Touch wiring and source values.](https://github.com/MiLacy/UniXNU-marble/blob/7f9a3e506235bf69479faf8f47dcda663fa9e658/docs/HARDWARE.md)

## GPU {#gpu}

`not-applicable`

### Hardware and dependencies

- Adreno 725 GPU and firmware; no catalog component covers this GPU generation.

### Configuration

- No GPU component is selected. CONFIG\_XNU\_NATIVE\_ADRENO is off.

### Procedure

- Inventory the GPU/GMU/firmware and define a submit/fence checkpoint before selecting a driver.

### Development findings

- The catalog Adreno740 component is SM8550-specific and does not cover the Adreno 725.

### Completion criteria

- A GPU driver matches the Adreno 725 and completes a known submission.

### Current limits

- No GPU acceleration is implemented or claimed.

### Next action

- Inventory the A725 GMU and SMMU interfaces from the vendor kernel before writing a driver.

### Evidence

Collect evidence using the next action.

## Network {#network}

`not-applicable`

### Hardware and dependencies

- Wi-Fi/cellular controller, firmware and transport.

### Configuration

- No network component is selected.

### Procedure

- Identify the exact WCN/ath12k part and its firmware before implementation.

### Development findings

- No network association or packet transfer is implemented.

### Completion criteria

- The device associates and transfers packets through the OS interface.

### Current limits

- No network support is claimed.

### Next action

- Identify the Wi-Fi part from the vendor DT and firmware partition.

### Evidence

Collect evidence using the next action.

## Audio {#audio}

`not-applicable`

### Hardware and dependencies

- ADSP firmware, codec, GPR/GLINK/SMEM transport and routes.

### Configuration

- No audio component is selected.

### Procedure

- Trace the DSP/codec dependencies and define a playback or loopback checkpoint.

### Development findings

- No DSP/codec route or playback/capture is implemented.

### Completion criteria

- At least one documented playback or capture route works.

### Current limits

- No audio support is claimed.

### Next action

- Inventory the ADSP and codec firmware before selecting a driver.

### Evidence

Collect evidence using the next action.

## Power {#power}

`planned`

### Hardware and dependencies

- PMIC, RPMh, battery, charger and CPU frequency domains.

### Configuration

- No power component is selected. The first console boots on external power.

### Procedure

- Add the PMGR placeholder for AMFI, then implement battery/charger and CPU frequency policy.

### Development findings

- Unmodified AMFI expects an ApplePMGR provider and panics at ConfigurationSettings.cpp:235 with \`No PMGR?\` when none exists. The board package now carries ShengPlatformPower.cpp and scripts/build-platform-power.py; CONFIG\_XNU\_SHENG\_PMGR=y plus sheng-pmgr=2 publishes the guarded placeholder from the original ApplePMGR ABI. It implements no hardware power operation and panics on any power call, so it is a bring-up bridge, not power management.
- No suspend/resume, battery or charger support exists in this image.

### Completion criteria

- Rails/clocks are owned, a supported power transition recovers, and battery readings match the device.

### Current limits

- No power management is implemented or claimed.

### Next action

- Run the PMGR-enabled image on hardware and record whether AMFI passes and which root/IOKit checkpoint follows.

### Evidence

- `source`: [PMGR placeholder and staged power work.](https://github.com/MiLacy/UniXNU-marble/blob/7f9a3e506235bf69479faf8f47dcda663fa9e658/docs/FULL-SYSTEM.md)

## Image assembly {#packaging}

`host-verified`

Components: `XnuPrelink`.

### Hardware and dependencies

- XNU source, iPadOS kernelcache, trust cache, Data seed, firmware DTB and UEFI FD.

### Configuration

- \[kernel\] prelink comes from the iPadOS 17.7.11 profile. \[payload\] selects a 192 MiB FAT volume, panic screen and the first-console arguments. \[firmware\] supplies the FD path.

### Procedure

- Build kernel, payload and boot image in order; inspect boot.json and payload.json before booting.

### Development findings

- The OS profile pins the same 21H461 kernelcache and trust caches used by sheng and alioth.
- The payload path is resolved relative to \\EFI\\UniXNU, so the builder writes \\EFI\\UniXNU\\Payloads\\kernel.macho; a file at the FAT root is not found.
- mformat -F produced a FAT32 BPB that the EDK2 FAT driver refused \(TotSec16 nonzero with TotSec32 zero\). The builder now creates FAT16 \(mkfs.fat -F 16, or mformat without -F\) and rejects FAT32 or a missing 0x55AA signature.
- UniXNURamDiskDxe validates the FAT boot sector, aligns initrd-end down to 512 bytes and calls ConnectController after installing BlockIo. UniXNU also connects all controllers before scanning for unixnu.toml, so the payload volume appears without relying on BDS enumeration order.
- The working fastboot boot path is an Android v0 image: kernel 0x80008000, ramdisk 0x80C00000, DTB /chosen initrd 0x80C00000-0x84C00000. The earlier v4 image did not load the ramdisk reliably.
- Host build 2026-10-30: kernel.macho SHA-256 e681a0b8e09f666703594025ccf13890db72a939c0f523c5cb51b340e5b8281a; the two-module original trust bundle SHA-256 7416ca4f8377146626bb284e2133c76bee6cb4e3ee0af5d648b186235a7323a7; payload.img 64 MiB \(FAT16\); boot.img 68947968 bytes with the Android v0 header, kernel at 0x80008000, ramdisk at 0x80C00000 and the marble firmware DTB.
- The payload FAT volume contains EFI/UniXNU/unixnu.toml, Payloads/kernel.macho, kernel.macho.adt and kernel.macho.trustcache; the empty-ramdisk Stage 1 image is 1839104 bytes.

### Completion criteria

- Kernel and prelink succeed, the payload contains all declared inputs, and the boot image matches the Android v0 header, the fixed ramdisk range and the 192 MiB partition.

### Current limits

- The userdata image is not built because the GPT geometry and Data seed are not present. No boot image has been run on hardware.

### Next action

- Run the Stage 2 real-device test with boot.img and report the DARWINBOOT lines and the first kernel lines.

### Evidence

- `host`: [Kernel, prelink, payload and both boot images built and verified on the host; hardware\_tested is false.](https://github.com/MiLacy/UniXNU-marble/blob/7f9a3e506235bf69479faf8f47dcda663fa9e658/README.md)
  Command: <code>./unixnu build kernel -D ../UniXNU-marble &amp;&amp; ./unixnu build payload -D ../UniXNU-marble --uefi ../Mu-Silicium-marble --trust-cache ../UniXNU-marble/build/marble-trust.bundle --unique-chip-id 4d4152424c45 &amp;&amp; ./unixnu bootimg -D ../UniXNU-marble --uefi ../Mu-Silicium-marble --dtb ../Mu-Silicium-marble/Resources/DTBs/marble-unixnu.dtb</code>. Revision: `f0721e9745c45ad2600cd69c74730f1cca55d56a`.

## Diagnostics {#diagnostics}

`planned`

### Hardware and dependencies

- Serial/UART access, firmware log exporter and panic screen.

### Configuration

- The first-console command line enables -v, serial=3 and debug=0x14e. panic\_screen is true. The firmware log exporter is not added yet.

### Procedure

- Capture the firmware log, the UniXNU menu counts, DarwinBoot output and the panic screen at each stage.

### Development findings

- The phone has no exposed UART; the framebuffer console and panic screen are the primary diagnostics.
- A QR log exporter is recommended before storage and kernel handoff; it is not integrated into the marble DSC/FDF yet.

### Completion criteria

- Each claimed milestone has a reproducible command, an exact image revision and an accessible log or photo.

### Current limits

- No real-device log has been captured.

### Next action

- Integrate Advanced Logger/AdvancedLogDxe and capture the Stage 1 menu log.

### Evidence

Collect evidence using the next action.

</div>
