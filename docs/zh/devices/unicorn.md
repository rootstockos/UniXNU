# Xiaomi 12S Pro \(unicorn, SM8475\): UniXNU UEFI bring-up

[zhouzhoublm/UniXNU-unicorn](https://github.com/zhouzhoublm/UniXNU-unicorn)

<div v-pre>

Xiaomi 12S Pro \(unicorn, SM8475\)：以 Pong/Thor 的格式2 方法从零移植 UniXNU，UEFI 已上屏（M1/M2 达成），内核与 payload 走 macOS CI

源记录: [porting.toml](https://github.com/zhouzhoublm/UniXNU-unicorn/blob/c069594dfffbf092df597379a7cf7575d1b59eaf/porting.toml), `c069594dfffbf092df597379a7cf7575d1b59eaf`.

本页按设备记录自动生成。

| 部分 | 状态 |
| --- | --- |
| [配置](#configuration) | `implemented` |
| [固件与启动](#firmware) | `device-verified` |
| [CPU 与中断](#cpu) | `implemented` |
| [内存与 DMA](#memory) | `implemented` |
| [存储与根卷](#storage) | `planned` |
| [显示](#display) | `planned` |
| [输入](#input) | `planned` |
| [GPU](#gpu) | `planned` |
| [网络](#network) | `planned` |
| [音频](#audio) | `planned` |
| [电源](#power) | `planned` |
| [镜像组装](#packaging) | `device-verified` |
| [诊断](#diagnostics) | `implemented` |

## 配置 {#configuration}

`implemented`

组件: `PongPlatform24`.

### 硬件与依赖

- 本机 root 采集的 35 份 dump 与 live device tree
- Pong/Thor 的格式2 四层配置（ARM64 → SM8475 → 18.7.10 → unicorn）
- iPad 10.2 18.7.10 \(22H374\) restore

### 配置

- board.toml 选 ipados-18.7.10-22H374 与 kit=unicorn
- configs/unicorn\_ipados18\_defconfig：ARMCPU=y + SOURCE\_CORECRYPTO14=y + OWNER\_\* 关闭
- dts/unicorn.dts：12 GiB @0x80000000、38 段 reserved-memory、触摸/按键/UFS/PCIe 节点

### 操作步骤

- 用 scripts/collect-device.sh 在 root 手机上只读采集
- scripts/bootstrap-unicorn.sh 重建 soc-seed 并 pin 框架到 0fc0fc1f
- scripts/audit-inherited-values.py 逐条核对没有沿用兄弟机型的数字

### 开发经验

- 所有板级数值都必须能追溯到本机采集，audit 脚本 13/13 通过后才允许进构建
- 格式2 要求 local.toml 只保留 soc + kit 两处本地覆盖，其余走 feeds.toml 的 git pin

### 完成条件

- bootstrap 在干净环境可重复产出同一套配置

### 当前范围

- 尚未在 CI 上复核 unicorn.dts 的每一段 reserved-memory 是否都必要

### 下一步

- 随 M3 一起用内核日志（ramoops/串口）复核保留区是否与实际使用一致

### 证据

- `source`: [四层配置与 IPSW 选择（iPad7,11 / 22H374 / j171ap）已写入并在 CI 上跑通 bootstrap](https://github.com/zhouzhoublm/UniXNU-unicorn/blob/c069594dfffbf092df597379a7cf7575d1b59eaf/board.toml)
- `source`: [13 条实测事实（面板、触摸、按键、UFS、PCIe、ramoops、userdata LBA 等）逐条有采集出处](https://github.com/zhouzhoublm/UniXNU-unicorn/blob/c069594dfffbf092df597379a7cf7575d1b59eaf/verified-facts.json)

## 固件与启动 {#firmware}

`device-verified`

组件: `PongPlatform24`.

### 硬件与依赖

- Mu-Silicium/EDK2（Mu\_Basecore + Silicon/Qualcomm + Silicium-ACPI）
- 本机 live DT 作为 FdtBlob
- K60\(mondrian\) 原版 MemoryMapLib（M2 已验证组合）

### 配置

- FD 基址 0xA7400000、长度 0x300000，PcdCPUCoresStackBase=0xA760D000
- ACPI 表暂时禁用（AML 不完整）
- 引导容器必须 Android boot header v4 + 1 MiB 占位 ramdisk

### 操作步骤

- WSL 里 unixnu build firmware --target RELEASE 产出 SILICIUM\_UEFI.fd
- scripts/make-m3-image.sh 用 build-bootimg.py 打 v4 容器
- fastboot boot 只进 RAM，不写任何分区

### 开发经验

- 用 adb 判断『固件是否活着』会误判：adb server 会返回幽灵设备，必须用 USB VID/PID 采样
- 菜单倒计时结束后若没有可启动项，会显示警示三角形并关机（走配置里的 shutdown 项），这不是崩溃
- ramdisk 为空时菜单必然没有启动项——启动项来自 ramdisk 里的 EFI/UniXNU/Payloads/kernel.macho

### 完成条件

- 屏幕上出现 UniXNU 菜单，且音量键能移动、电源键能确认

### 当前范围

- 面板初始化参数（DCS/DSI）尚未按本机 LTPO 面板逐项核对

### 下一步

- M3 通过后恢复 ACPI 表（补齐 AML）并验证 USB 键盘

### 证据

- `device`: [fastboot boot 后屏幕出现 UniXNU 菜单；硬件侧 90 秒无 USB 枚举，确认固件在运行](https://github.com/zhouzhoublm/UniXNU-unicorn/blob/c069594dfffbf092df597379a7cf7575d1b59eaf/docs/evidence/m1-menu.md)
  Command: <code>fastboot boot out/unicorn-uefi-M1f-goldfd.img</code>. Revision: `1b2a355607fe9d7691c6108cf889d9d5da64157a`.
- `source`: [bootstrap 在 GitHub macOS runner 上跑通：pin / soc-seed / profile / defconfig / fetch 全部成功](https://github.com/zhouzhoublm/UniXNU-unicorn/blob/c069594dfffbf092df597379a7cf7575d1b59eaf/scripts/bootstrap-unicorn.sh)

## CPU 与中断 {#cpu}

`implemented`

组件: `PongPlatform24`.

### 硬件与依赖

- SM8475 8 核（1×X3 + 4×A715/A710 + 3×A510）
- Pong 的 MACHINE\_CONFIGS=PONG 机器配置

### 配置

- ARMCPU=y，使用真实 ArmCpu 中断提供者
- cpus=1 作为首启参数，降低早期并发风险

### 操作步骤

- defconfig 打开 ARMCPU 与 SOURCE\_CORECRYPTO14，关闭 OWNER\_\* 以满足离线 SystemOS 的独立 root 前提

### 开发经验

- 缺 ARMCPU 会在构造期被守卫拒绝：Architectural timer entry requires the actual ArmCpu interrupt provider
- MACHINE\_CONFIGS 必须是 PONG；BCM2837 会报 Unsupported CURRENT\_MACHINE\_CONFIG

### 完成条件

- 内核在单核下进入 launchd

### 当前范围

- 多核启动与 GIC 亲和性尚未验证

### 下一步

- M3 后按核逐个放开并观察中断分布

### 证据

- `source`: [ARMCPU=y、SOURCE\_CORECRYPTO14=y、OWNER\_\* 关闭；CI 上据此完成内核配置](https://github.com/zhouzhoublm/UniXNU-unicorn/blob/c069594dfffbf092df597379a7cf7575d1b59eaf/configs/unicorn_ipados18_defconfig)

## 内存与 DMA {#memory}

`implemented`

组件: `PongPlatform24`.

### 硬件与依赖

- MemTotal 11,444,364 kB（12 GiB）
- 本机 /proc/iomem 与 live DT 的 reserved-memory

### 配置

- DDR 从 0x80000000 起 12 GiB
- 38 段 reserved-memory 原样进 DT
- userdata 分区 first\_lba 0x2AE13BB / last\_lba 0x3AE1F7A / lba\_bytes 4096

### 操作步骤

- 从 live DT 直接抽取 reserved-memory 生成 dts/unicorn.dts
- UEFI 侧沿用 mondrian 的 K60 MemoryMapLib（M2 已点亮组合），并保留 ABL staging 窗口

### 开发经验

- 我们按 reserved-memory 自己生成的 MemoryMapLib 在 M2 对照实验中黑屏，K60 原版反而点亮——说明 UEFI 侧内存描述符需要逐项二分，暂以 K60 表为基线
- ramoops 位于 0xA7000000 + 0x400000，FD 取 0xA7400000 正好避开

### 完成条件

- UEFI 能稳定运行且内核能接管同一份内存图

### 当前范围

- unicorn 自有的 trusted\_apps 两段（0xEA000000 / 0xED900000）在 UEFI 表里尚未纳入

### 下一步

- M3 后用内核日志二分修正 MemoryMapLib，替换 K60 表

### 证据

- `source`: [12 GiB @0x80000000、38 段 reserved-memory、userdata LBA 均来自本机采集](https://github.com/zhouzhoublm/UniXNU-unicorn/blob/c069594dfffbf092df597379a7cf7575d1b59eaf/dts/unicorn.dts)
- `device`: [使用 K60 MemoryMapLib 的固件在本机能出菜单并稳定运行 90 秒以上](https://github.com/zhouzhoublm/UniXNU-unicorn/blob/c069594dfffbf092df597379a7cf7575d1b59eaf/docs/evidence/m1-menu.md)
  Command: <code>fastboot boot out/unicorn-uefi-M1f-goldfd.img</code>. Revision: `1b2a355607fe9d7691c6108cf889d9d5da64157a`.

## 存储与根卷 {#storage}

`planned`

组件: `PongPlatform24`.

### 硬件与依赖

- UFS 1d84000.ufshc，SPI 265，SMMU SID 0xe0，2 lanes，reset GPIO 210
- userdata 为 sda36（start 24104960，size 469749720，512B 扇区）

### 配置

- dts 里 &amp;ufs 已标注 SID/SPI 与 lane 数
- bootimg 侧 userdata LBA 已按 4K 扇区换算

### 操作步骤

- M3 通过后启用 NativeUfs 驱动并挂载 SystemOS 卷

### 开发经验

- 分区信息来自 root 手机的 block-by-name 与 gpt dump，非推测

### 完成条件

- 内核能读写 userdata 并挂上 APFS 卷

### 当前范围

- 尚未在真机上验证 UFS 驱动

### 下一步

- 内核起来后先读分区表再尝试挂载

### 证据

- `source`: [&amp;ufs 节点记录了 SMMU SID 0xe0、SPI 265、2 lanes 与 reset GPIO 210](https://github.com/zhouzhoublm/UniXNU-unicorn/blob/c069594dfffbf092df597379a7cf7575d1b59eaf/dts/unicorn.dts)

## 显示 {#display}

`planned`

组件: `PongPlatform24`.

### 硬件与依赖

- 面板 qcom,mdss\_dsi\_l2s\_38\_0c\_0a\_dsc\_cmd，1440×3200，DSC 10bpc，LTPO 120/90/60/40/30/24 Hz
- UEFI 侧 GOP 已能在屏上画出菜单

### 配置

- UEFI 阶段靠 SimpleFramebuffer 出画面，Apple 侧显示栈尚未接入

### 操作步骤

- 先在 UEFI 验证能出画，再逐步补面板 DCS/DSI 初始化序列

### 开发经验

- UEFI 菜单已经上屏，说明最基本的帧缓冲通路是通的；风险集中在 Apple 显示驱动的面板时序

### 完成条件

- 内核阶段屏幕能显示进度条/panic 文本

### 当前范围

- 面板 DCS 序列未逐条核对，是整个移植最大的单点风险

### 下一步

- 从本机 display dumpsys/sysfs 里补齐面板初始化序列

### 证据

- `source`: [面板型号、分辨率、DSC 位深与刷新率档位均来自本机 display 节点](https://github.com/zhouzhoublm/UniXNU-unicorn/blob/c069594dfffbf092df597379a7cf7575d1b59eaf/verified-facts.json)

## 输入 {#input}

`planned`

组件: `PongPlatform24`.

### 硬件与依赖

- 触摸 FocalTech FTS，SPI 0x990000，IRQ GPIO 21，RST GPIO 20，12 MHz，x-max 14400 / y-max 32000
- PON PPID 0x13，音量上 TLMM GPIO 6

### 配置

- dts 里 unicorn-touch 与 unicorn-buttons 节点已写入上述数值

### 操作步骤

- UEFI 先用 PON/GPIO 按键驱动菜单，再在 Apple 侧接触摸

### 开发经验

- UEFI 菜单里音量键可以移动、电源键可以确认，说明按键通路已经可用
- unicorn 的 PON PPID 与音量 GPIO 与 pong 不同，不能沿用 pong 的数值

### 完成条件

- XNU 侧触摸与按键都能工作

### 当前范围

- Apple 侧触摸驱动尚未开始

### 下一步

- M3 后先接按键，再接触摸

### 证据

- `device`: [菜单中音量键移动光标、电源键确认，实体键通路可用](https://github.com/zhouzhoublm/UniXNU-unicorn/blob/c069594dfffbf092df597379a7cf7575d1b59eaf/docs/evidence/m1-menu.md)
  Command: <code>fastboot boot out/unicorn-uefi-M1e-v4ramdisk.img</code>. Revision: `1b2a355607fe9d7691c6108cf889d9d5da64157a`.
- `source`: [触摸与按键的 GPIO/PPID 均来自本机 input dumps](https://github.com/zhouzhoublm/UniXNU-unicorn/blob/c069594dfffbf092df597379a7cf7575d1b59eaf/verified-facts.json)

## GPU {#gpu}

`planned`

组件: `PongPlatform24`.

### 硬件与依赖

- Adreno 730 v3
- Pong 社区的 AdrenoMetal/ArmCpu 等 12 个驱动子模块（按 thor pin）

### 配置

- drivers/ 按 thor 的 pin 拉齐，路径名与 pong 一致

### 操作步骤

- M4 之后再接 Metal；先让 launchd 起来

### 开发经验

- GPU 相关驱动依赖内核先稳定，当前不作为 M3 的阻塞项

### 完成条件

- Metal 能完成最小渲染

### 当前范围

- 尚未开始

### 下一步

- 内核稳定后按 thor 的 Metal 记录移植

### 证据

- `source`: [drivers/ 下 12 个驱动以 submodule 引用，路径名与 pong 一致、pin 与 thor 对齐](https://github.com/zhouzhoublm/UniXNU-unicorn/blob/c069594dfffbf092df597379a7cf7575d1b59eaf/.gitmodules)

## 网络 {#network}

`planned`

组件: `PongPlatform24`.

### 硬件与依赖

- Wi-Fi QCA6490，PCIe RC0 0x1c00000，wlan-en GPIO 80，perst 94，wake 96，clkreq 95

### 配置

- dts 里 &amp;pcie0 已带上述 GPIO

### 操作步骤

- M4 之后接 Wi-Fi/BT

### 开发经验

- 无线在首启阶段不是必需项

### 完成条件

- 能扫描到 AP

### 当前范围

- 尚未开始

### 下一步

- 内核稳定后接 QCA6490

### 证据

- `source`: [QCA6490 的 PCIe 基址与 4 个 GPIO 来自本机 wifi/PCIe 节点](https://github.com/zhouzhoublm/UniXNU-unicorn/blob/c069594dfffbf092df597379a7cf7575d1b59eaf/verified-facts.json)

## 音频 {#audio}

`planned`

组件: `PongPlatform24`.

### 硬件与依赖

- SM8475 ADSP 音频子系统（沿用 Pong 方案）

### 配置

- 当前未接任何 codec

### 操作步骤

- M5 之后再考虑

### 开发经验

- 音频对首启不是必需项，明确留在最后

### 完成条件

- 能播放提示音

### 当前范围

- 尚未开始

### 下一步

- 等 SystemOS 起来后再评估

### 证据

- `source`: [音频与蓝牙列为 M6 之后的次要项](https://github.com/zhouzhoublm/UniXNU-unicorn/blob/c069594dfffbf092df597379a7cf7575d1b59eaf/PORTING-TODO.md)

## 电源 {#power}

`planned`

组件: `PongPlatform24`.

### 硬件与依赖

- PMIC（pon-ppid 0x13）、充电与电池节点来自本机 power dump

### 配置

- UEFI 阶段可正常关机（ResetSystem EfiResetShutdown 已验证）

### 操作步骤

- 内核阶段接 PMIC 与充电驱动

### 开发经验

- 菜单超时后能正常关机，说明 ResetSystem 通路可用

### 完成条件

- XNU 下能正确上报电量与温度

### 当前范围

- Apple 侧电源管理尚未开始

### 下一步

- M3 后接 PMIC

### 证据

- `source`: [PON PPID 与电源节点来自本机 power/keys 采集](https://github.com/zhouzhoublm/UniXNU-unicorn/blob/c069594dfffbf092df597379a7cf7575d1b59eaf/verified-facts.json)

## 镜像组装 {#packaging}

`device-verified`

组件: `PongPlatform24`.

### 硬件与依赖

- RELEASE 固件（SILICIUM\_UEFI.fd，3 MiB）
- 本机 FdtBlob DTB（895,141 B）
- payload.img（FAT，含 EFI/UniXNU/Payloads）

### 配置

- 容器固定 Android boot header v4 + header\_size 1584
- ramdisk 必须是 payload，不能是空占位
- 追加 DTB 使用本机 live DT

### 操作步骤

- scripts/make-m3-image.sh --kernel/--payload 生成并自校验（v4、DTB 魔数、ramdisk 内容标记）
- CI 侧 kernel-macos 产出 kernel.macho 与 payload.img，本地组装成镜像

### 开发经验

- 框架自带 unixnu bootimg 只支持 v0–v2 头，本机 ABL 必须 v4，所以改用固件仓的 build-bootimg.py
- 1 MiB 全零 ramdisk 的镜像能出菜单但没有启动项；换成含 payload 的 ramdisk 后菜单出现 4 项

### 完成条件

- 镜像结构自校验通过且能在真机上被 ABL 接受

### 当前范围

- 还没跑过带 darwin24 payload 的镜像（等 CI 产物）

### 下一步

- 拿到 CI 的 kernel.macho/payload.img 后打 M3 候选镜像

### 证据

- `device`: [带旧 payload 的镜像在本机菜单里显示出 4 个启动项（UniXNU/Restart/Power off/Tools）](https://github.com/zhouzhoublm/UniXNU-unicorn/blob/c069594dfffbf092df597379a7cf7575d1b59eaf/docs/evidence/m3-payload-menu.md)
  Command: <code>fastboot boot out/unicorn-uefi-M3-payload.img</code>. Revision: `1b2a355607fe9d7691c6108cf889d9d5da64157a`.
- `source`: [脚本自校验：boot v4、header 1584、DTB 魔数 d00dfeed、ramdisk 含 kernel.macho/Payloads](https://github.com/zhouzhoublm/UniXNU-unicorn/blob/c069594dfffbf092df597379a7cf7575d1b59eaf/scripts/make-m3-image.sh)

## 诊断 {#diagnostics}

`implemented`

组件: `PongPlatform24`.

### 硬件与依赖

- 固件阶段没有任何日志通道（XNU 未起来，UEFI 日志不落盘）
- 主机侧只能观察 USB 枚举

### 配置

- 用 USB VID/PID 采样判断固件是否存活；用屏摄判断菜单与 panic

### 操作步骤

- scripts/boot-observe.ps1：fastboot boot 后每 3–6 秒采一次 PnP 设备 ID
- scripts/audit-inherited-values.py：核对每个数值都有本机来源

### 开发经验

- adb devices 会返回幽灵设备，用它判断固件存活会得出完全相反的结论
- 手机进 UEFI 后 USB 全断属正常；出现 VID\_05C6&amp;PID\_F000 是空介质存储 gadget

### 完成条件

- 每次上机都能给出可复现的判定证据

### 当前范围

- 还没有 UEFI 日志落盘通道，屏幕仍是唯一的内核前证据

### 下一步

- 后续加 AdvancedLogger 落盘或 QR 导出，减少对屏摄的依赖

### 证据

- `source`: [13/13 项核对通过，确保没有沿用兄弟机型的数值](https://github.com/zhouzhoublm/UniXNU-unicorn/blob/c069594dfffbf092df597379a7cf7575d1b59eaf/scripts/audit-inherited-values.py)
- `device`: [USB PnP 采样显示 boot 后 3–42 秒无任何 USB 设备，与『固件在运行』一致](https://github.com/zhouzhoublm/UniXNU-unicorn/blob/c069594dfffbf092df597379a7cf7575d1b59eaf/docs/evidence/m1-menu.md)
  Command: <code>fastboot boot out/unicorn-uefi-M1e-v4ramdisk.img</code>. Revision: `1b2a355607fe9d7691c6108cf889d9d5da64157a`.

</div>
