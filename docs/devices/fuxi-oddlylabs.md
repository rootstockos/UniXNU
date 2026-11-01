# Xiaomi 13 \(OddlyLabs\)

[OddlyLabs/unixnu-fuxi](https://github.com/OddlyLabs/unixnu-fuxi)

<div v-pre>

Xiaomi 13 \(fuxi\). Board files from the UniXNU device template, the FTM5 touch driver and its staged kernel probe. No device boot evidence yet.

Source record: [porting.toml](https://github.com/OddlyLabs/unixnu-fuxi/blob/8b4c03f69fc15878d7ee92c0f157215fa235d78d/porting.toml), `8b4c03f69fc15878d7ee92c0f157215fa235d78d`.

This page is generated from the device record.

| Area | Status |
| --- | --- |
| [Configuration](#configuration) | `implemented` |
| [Firmware and boot](#firmware) | `planned` |
| [CPU and interrupts](#cpu) | `planned` |
| [Memory and DMA](#memory) | `planned` |
| [Storage and root](#storage) | `planned` |
| [Display](#display) | `planned` |
| [Input](#input) | `implemented` |
| [GPU](#gpu) | `planned` |
| [Network](#network) | `planned` |
| [Audio](#audio) | `planned` |
| [Power](#power) | `planned` |
| [Image assembly](#packaging) | `planned` |
| [Diagnostics](#diagnostics) | `implemented` |

## Configuration {#configuration}

`implemented`

Components: `ArmDTPlatform`, `ArmCpu`, `SoftwareAES`, `XnuPrelink`, `DarwinBoot`.

### Hardware and dependencies

- UniXNU examples/device template \(SM8550, iPadOS 17.7.11 profile\).
- UniXNU-sm8550-common at 23b2049fcd9e30356161def84231334fb55ecca9.

### Configuration

- board.toml: name fuxi, kit fuxi, soc\_source sm8550, os\_profile ipados-17.7.11-21H461, device\_tree.source device:dts/fuxi.dts.
- configs/fuxi\_defconfig: the template options, CONFIG\_XNU\_NATIVE\_ADRENO off \(the SoC fragment turns it on\), and CONFIG\_XNU\_EXTRA\_LD\_ARCHIVES for the touch archive. The arch fragment sets CONFIG\_XNU\_BUILD\_ARCHIVES, CONFIG\_XNU\_NATIVE\_AES, CONFIG\_XNU\_NATIVE\_ARCH\_TIMER and CONFIG\_XNU\_NATIVE\_PAN; the OS fragment sets CONFIG\_XNU\_SOURCE\_MOCKFS.
- \[kernel\] archives in build order: corecrypto \(plain and namespaced\), ArmDTPlatform, ArmCpu, SoftwareAES and its bindings, libpthread, the mockfs fixture, build-fuxi-touch.py.

### Procedure

- ./unixnu defconfig fuxi\_defconfig -D ../unixnu-fuxi
- ./unixnu status --json
- ./unixnu build kernel

### Development findings

- The merged configuration is the arch, SoC and OS fragments followed by the defconfig; the archive list has to cover every archive the merged options require, not only those of the template defconfig.
- CONFIG\_XNU\_SOURCE\_MOCKFS \(OS fragment\) makes vfs\_conf.o reference mockfs, so the link needs build/mockfs-fixture/libmockfs\_fixture.a: the first CI kernel build at 450f518 applied all 62 patches and built every listed archive, then stopped at that missing archive.
- ArmCpu needs the source corecrypto provider, so corecrypto is built before the components, as in UniXNU-sheng.

### Completion criteria

- All source pins resolve; every selected component has a record; status shows the intended overrides and patch order.

### Current limits

- unixnu.lock is not committed yet.

### Next action

- Run ./unixnu fetch and commit unixnu.lock.

### Evidence

- `source`: [Board selection and archive order](https://github.com/OddlyLabs/unixnu-fuxi/blob/8b4c03f69fc15878d7ee92c0f157215fa235d78d/board.toml)

## Firmware and boot {#firmware}

`planned`

### Hardware and dependencies

- A fuxi UEFI port that starts DarwinBoot: hanzi123-bilp/uefi-fuxi, Sohytrs/uefi-fuxi \(fuxi branch\) or mouse233/uefi-fuxi.
- fastboot boot of the image \(temporary, nothing is written to the device\).

### Configuration

- board.toml \[firmware\]: config fuxi, dsc Platforms/Xiaomi/fuxiPkg/fuxi.dsc, as the community UEFI ports use; not built here yet.

### Procedure

- Select and pin a uefi-fuxi revision, then ./unixnu build firmware --uefi ../uefi-fuxi.

### Development findings

- The stock device tree already has /reserved-memory/splash\_region at 0xb8000000 \(0x2b00000\), so no bootimg dtb\_node is added.

### Completion criteria

- Firmware image builds, enters UniXNU, publishes the framebuffer and preserves the documented memory reservations.

### Current limits

- No firmware revision is selected and no firmware build has run.

### Next action

- Pin one uefi-fuxi revision and build it.

### Evidence

Collect evidence using the next action.

## CPU and interrupts {#cpu}

`planned`

### Hardware and dependencies

- CPU layout and GIC ranges of SM8550 v2 \(soc\_id 519\) from the stock device tree.

### Configuration

- The template ArmCpu and ArmDTPlatform selection.

### Procedure

- Boot to the XNU entry checkpoint and record the enabled CPUs.

### Development findings

- No fuxi CPU observation yet.

### Completion criteria

- Every enabled CPU reaches its entry checkpoint and timer/IPI handling works on each enabled CPU.

### Current limits

- Hardware inputs are not recorded in dts/fuxi.dts beyond the SoC DTSI.

### Next action

- Record the CPU and GIC nodes from the stock device tree.

### Evidence

Collect evidence using the next action.

## Memory and DMA {#memory}

`planned`

### Hardware and dependencies

- DRAM and reserved-memory nodes of the stock device tree; the apps SMMU at 0x15000000.

### Configuration

- dts/fuxi.dts includes sm8550.dtsi and sets model and compatible; package/SmmuBypass.h is the UniXNU-sheng pass-through stream helper the touch probe uses for stream 0xa3.

### Procedure

- Fill the memory map from the stock device tree and the firmware's memory map.

### Development findings

- fuxi and sheng share the touch engine's SMMU stream \(0xa3\).

### Completion criteria

- All handoff regions are accounted for and each enabled DMA engine accesses its allocated range with the documented cache policy.

### Current limits

- The DTS memory map is the SoC default.

### Next action

- Record the DRAM size and reservations of the device.

### Evidence

Collect evidence using the next action.

## Storage and root {#storage}

`planned`

### Hardware and dependencies

- UFS host of SM8550; the GPT of LUN 0 \(userdata\) on the device.

### Configuration

- CONFIG\_XNU\_NATIVE\_UFS is not set.

### Procedure

- Select a UFS component after the firmware boots.

### Development findings

- Other fuxi ports report the UFS controller hand-off from the firmware as usable; not reproduced here.

### Completion criteria

- Host: register, clock, link and SMMU facts match the board inventory.

### Current limits

- No storage driver is selected.

### Next action

- Choose the root path \(RAM root or UFS\) once the firmware boots.

### Evidence

Collect evidence using the next action.

## Display {#display}

`planned`

### Hardware and dependencies

- Panel xiaomi m3 38 0c 0a \(Samsung DDIC by its command set\), 1080x2400, DSI0 command mode with DSC 8 bpp, slices 540x40, 1.1 GHz.
- Splash framebuffer 0xb8000000 \(0x2b00000\), 32 bpp, stride 1080 pixels, the same region as sheng.

### Configuration

- package/FuxiPanelBoard.h holds the panel and controller constants with their sources; no display component is selected.

### Procedure

- First stage: keep the firmware framebuffer \(GopFramebuffer route\), as sheng does.

### Development findings

- 73 DSI command sequences of the panel are decoded in facts/panel\_cmds\_\*.txt.
- The PPS in the on-command gives pic\_height 2520 while the timing and page address give 2400; unresolved.

### Completion criteria

- A known frame appears with correct geometry and colors, then userland presentation and backlight controls work.

### Current limits

- No device evidence for display.

### Next action

- Read the framebuffer the firmware publishes on fuxi \(address, stride, format\).

### Evidence

- `source`: [Panel facts with sources \(section 3\)](https://github.com/OddlyLabs/unixnu-fuxi/blob/8b4c03f69fc15878d7ee92c0f157215fa235d78d/fuxi_touch_panel_porting.md)

## Input {#input}

`implemented`

Components: `QcomGeniSpi`, `FuxiFtsTouch`.

### Hardware and dependencies

- Stock device tree \(OS3.0.309 vendor\_boot DTB\[2\] with dtbo\[4\], merged with fdtoverlay\): /soc/qcom,qupv3\_1\_geni\_se@ac0000/spi@a90000/fts@0, compatible st,spi, 12 MHz, SPI mode 0.
- Controller ST FingerTip FTM5, chip id 36 40, firmware 53.42 kept in its own flash \(/proc/tp\_fw\_version on Android\), tp-vendor 0x38.
- Bus QUP1 SE4 at 0xa90000, wrapper 0xac0000, SMMU stream 0xa3; pins 44-47 \(qup1\_se4\); interrupt GPIO 25 \(active low, TLMM summary SPI 208\); reset GPIO 24.
- Rails vdd ldob12 1.8 V \(RPMh 0x40900\) and avdd ldom6 3.2 V \(RPMh 0x41b00\), measured with root on Android; their only consumer is spi0.0.

### Configuration

- board.toml \[components\] selects QcomGeniSpi and FuxiFtsTouch; \[kernel\] archives runs scripts/build-fuxi-touch.py.
- configs/fuxi\_defconfig links build/fuxi-touch/libFuxiTouch.a through CONFIG\_XNU\_EXTRA\_LD\_ARCHIVES; patches/xnu/fuxi-touch-probe.patch calls native\_touch\_probe\(\) from bsd\_init after IOKitInitializeTime\(\).
- Boot arguments: fuxi-touch=1..4, fuxi-touch-panic=1 or 2, fuxi-touch-hz, fuxi-touch-power, fuxi-touch-tcs, fuxi-touch-poll, fuxi-touch-map. board.toml sets fuxi-touch=3 fuxi-touch-panic=1.

### Procedure

- Stage 1: snapshot of the QUP clocks, the pins and the two rails in the command database.
- Stage 2: RPMh votes vdd then avdd in active TCS 2 of apps RSC DRV2 \(tcs-offset 0xd00\), wait 3 ms, vote the QUP clocks, configure the pins.
- Stage 3: GENI SPI at 12 MHz; reset low 10 ms; wait for event 03 \(2 ms polls, 100 ms, three tries, then the ICR 4-wire fallback and the chip id\); read the chip id.
- Stage 4: A0 00 00, 50 ms, A0 00 01; falling-edge interrupt; drain the FIFO; publish contacts in the ShengHidInput ABI.
- Host: run the FuxiFtsTouch tests and tests/board\_test.c \(README\).

### Development findings

- The fts\_spi driver dispatches events on the high nibble of byte 0, whose bits 2-3 carry the top of the major axis, so 0x13 to 0x1F are all enter events.
- Coordinates are ten times the panel pixels \(fts,support-super-resolution\): 0..10800 x 0..24000; the probe maps them to the 30480 x 20320 range that ShengHidInput scales from.
- Both touch rails stay on while Android runs, also with the screen off, and carry no boot-on property, so the probe votes them before it touches the controller.

### Completion criteria

- Touches map to the display, simultaneous contacts keep stable IDs, release events arrive, and buttons produce expected HID reports.

### Current limits

- No device evidence. Unverified: whether the RPMh done bit sets without the TCS interrupt enabled, the axis directions, and the HID orientation on a portrait panel.
- GENI has no setting for the 10 us chip-select hold the vendor driver uses \(SPI\_DELAY\_CS\).

### Next action

- Boot an image with fuxi-touch=3 fuxi-touch-panic=1 and read the FTS rows on the panic screen: reset result 0 and chip id 36 40.

### Evidence

- `source`: [FTM5 protocol and contact tracking with host tests](https://github.com/OddlyLabs/FuxiFtsTouch)
- `source`: [Staged board probe](https://github.com/OddlyLabs/unixnu-fuxi/blob/8b4c03f69fc15878d7ee92c0f157215fa235d78d/package/FuxiTouchProbe.cpp)
- `source`: [Touch facts with sources and the Android measurements \(sections 2 and 6\)](https://github.com/OddlyLabs/unixnu-fuxi/blob/8b4c03f69fc15878d7ee92c0f157215fa235d78d/fuxi_touch_panel_porting.md)

## GPU {#gpu}

`planned`

### Hardware and dependencies

- Adreno 740 of SM8550, the same GPU as sheng.

### Configuration

- CONFIG\_XNU\_NATIVE\_ADRENO is not set.

### Procedure

- Reuse the sheng GPU components after display and storage work.

### Development findings

- No fuxi GPU observation yet.

### Completion criteria

- GPU submissions complete, mapping/fence handling is correct, a known image renders, and the selected Metal client presents it.

### Current limits

- Not started.

### Next action

- None before the first boot.

### Evidence

Collect evidence using the next action.

## Network {#network}

`planned`

### Hardware and dependencies

- The Wi-Fi chip of the board, from the stock device tree.

### Configuration

- No network component is selected.

### Procedure

- Identify the Wi-Fi chip and its bus.

### Development findings

- No fuxi network observation yet.

### Completion criteria

- Scanning, association, address configuration and sustained receive/transmit traffic pass with the selected firmware.

### Current limits

- Not started.

### Next action

- None before the first boot.

### Evidence

Collect evidence using the next action.

## Audio {#audio}

`planned`

### Hardware and dependencies

- The audio DSP and codec of the board.

### Configuration

- No audio component is selected.

### Procedure

- Identify the codec and amplifiers from the stock device tree.

### Development findings

- No fuxi audio observation yet.

### Completion criteria

- Each declared route carries the selected audio format through playback or capture and handles start/stop correctly.

### Current limits

- Not started.

### Next action

- None before the first boot.

### Evidence

Collect evidence using the next action.

## Power {#power}

`planned`

### Hardware and dependencies

- RPMh command database \(/sys/kernel/debug/cmd-db on Android\) and the regulator summary with the screen on and off.

### Configuration

- package/QcomRpmh.c and QcomRpmh.h are the UniXNU-sheng RPMh helpers; the touch probe uses them for its two rails.

### Procedure

- Record each rail a driver needs from the command database and the regulator summary.

### Development findings

- Panel rails vci 3.0 V, vddio 1.8 V and vddd 1.5 V are off while the screen is off; the touch rails stay on.

### Completion criteria

- Declared power controls and battery readings match observations; each claimed sleep/resume transition restores its dependent peripherals.

### Current limits

- Only the touch rails are used.

### Next action

- None before the first boot.

### Evidence

Collect evidence using the next action.

## Image assembly {#packaging}

`planned`

### Hardware and dependencies

- The iPadOS 17.7.11 restore image of the OS profile, the unique chip id of the device, a firmware image.

### Configuration

- patches/xnu/series applies fuxi-touch-probe.patch after the arch, SoC and OS series.
- \[payload\] kernel\_args set the touch probe stage and its panic report; panic\_screen = true.

### Procedure

- ./unixnu build kernel, then ./unixnu build payload --unique-chip-id &lt;hex&gt;, then ./unixnu bootimg --uefi ../uefi-fuxi.

### Development findings

- fuxi-touch-probe.patch keeps only original XNU lines as context, so changes to the shared series around it do not break it.

### Completion criteria

- Kernel and prelink succeed, payload contains all declared inputs, and the boot image matches the bootloader format and size limit.

### Current limits

- The payload needs the device's unique chip id; no payload or boot image has been built.

### Next action

- Build the kernel in CI, then the payload once the chip id and firmware are chosen.

### Evidence

Collect evidence using the next action.

## Diagnostics {#diagnostics}

`implemented`

### Hardware and dependencies

- The panic screen of the payload \(panic\_screen = true\) while no display driver runs.

### Configuration

- fuxi-touch-panic=1 ends the boot with a panic whose message is the probe's four diagnostic rows; the rows are also printed to the kernel log at each stage.

### Procedure

- Photograph the panic screen and compare the FTS rows with fuxi\_touch\_panel\_porting.md section 7.2.

### Development findings

- The other fuxi ports read their results from panic screen photographs as well.
- The earlier ci/ scripts compiled the probe inside a UniXNU-sheng build; the fuxi kernel build replaces them.

### Completion criteria

- Each claimed milestone has a reproducible command, a matching source/image revision and an accessible result.

### Current limits

- No device log has been captured.

### Next action

- Capture the first touch report from the device.

### Evidence

- `source`: [Panic report and kernel log rows](https://github.com/OddlyLabs/unixnu-fuxi/blob/8b4c03f69fc15878d7ee92c0f157215fa235d78d/package/FuxiTouchProbe.cpp)

</div>
