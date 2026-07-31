# Goodix 55x4 指纹传感器 Linux 驱动

[![CI](https://github.com/GuNanOvO/goodix-55x4-linux/actions/workflows/build.yml/badge.svg)](https://github.com/GuNanOvO/goodix-55x4-linux/actions/workflows/build.yml)

Goodix 55x4 系列（GF3208/GF3268）指纹传感器 Linux 驱动。USB `27c6:55a4`、`27c6:55b4`，常见于 Lenovo ThinkBook、ThinkPad、IdeaPad 及淘宝 DIY 模块。

基于 [TheWeirdDev/libfprint](https://github.com/TheWeirdDev/libfprint)、[jedbillyb/libfprint](https://github.com/jedbillyb/libfprint/tree/goodix-55b4-fixes) 和 [mpi3d/goodix-fp-dump](https://github.com/mpi3d/goodix-fp-dump) 的成果。

[English README](../README.md) · [编译指南](BUILDING.md) · [技术细节](TECHNICAL.md) · [安全说明](SECURITY.md) · [逆向记录](RE_NOTES.md)

---

## 背景

汇顶（Goodix）不提供 Linux 驱动。其传感器出厂固件仅适配 Windows（Intel SGX PSK 方案 + WBDI 框架）。截至 2026 年，**上游 libfprint 未支持 55x4 系列**——Debian/Ubuntu 打包的 libfprint 将 `27c6:55a4` 列为"已知但不支持"。

## 方案

本驱动替换系统 libfprint，提供：

| 能力 | 说明 |
|------|------|
| **硬件识别** | 通过 USB ID 发现传感器（`27c6:55a4`、`27c6:55b4`） |
| **固件兼容** | 接受任意 `GF32xx_RTSEC_APP_*` 版本（10041、10056、10062 等） |
| **PSK 自动补给** | 检测到 Windows 写入的 PSK 后自动写入社区密钥，无需手动操作 |
| **OpenSSL 3.x** | 修复 PSK 加密套件在 OpenSSL 3.x 下被拒的问题 |
| **双系统安全** | systemd 服务在启动时自动校准 PSK，Windows/Linux 切换无感 |
| **TLS 会话预热** | 跨验证重试缓存 TLS 会话，瞬时重新认证 |

## 支持硬件

| USB ID | 芯片 | 已验证平台 |
|--------|------|-----------|
| `27c6:55a4` | GF3208 / GF3268 | Debian forky/sid, Lenovo ThinkBook 16 G4+ IAP |
| `27c6:55b4` | GF3268 | Void Linux, Lenovo IdeaPad Flex 5 |
| `27c6:5584` | GF3268 | 待确认 |

## 安装

### 快速：预编译 .deb

> **注意：** Release 产物由 GitHub Actions 自动构建，**未经 GPG 签名**。生产环境建议从源码编译。Release 页面提供 SHA256 校验和验证。

```bash
wget https://github.com/GuNanOvO/goodix-55x4-linux/releases/latest/download/goodix-gf32xx-driver_0.1.0_ubuntu-24.04_amd64.deb
wget https://github.com/GuNanOvO/goodix-55x4-linux/releases/latest/download/SHA256SUMS
sha256sum -c SHA256SUMS  # 校验完整性
sudo dpkg -i goodix-gf32xx-driver_*.deb
sudo apt install --fix-broken -y
```

根据系统选择 `ubuntu-24.04` 或 `ubuntu-22.04`。

### 从源码编译

```bash
git clone https://github.com/GuNanOvO/goodix-55x4-linux.git
cd goodix-55x4-linux
sudo ./tools/install.sh
```

安装脚本自动处理依赖、编译驱动、写入 udev 规则并启用 PSK 自动修复服务。详见 [BUILDING.md](BUILDING.md)。

### 一次性：刷固件

> **警告：** 固件刷写工具以 root 权限运行第三方 Python 代码，直接与 USB 传感器通信。执行前请审阅 [mpi3d/goodix-fp-dump](https://github.com/mpi3d/goodix-fp-dump) 源码。刷写前工具会自动备份当前固件。

如果你的传感器从未被本驱动接触过，可能需要刷入社区固件**一次**。这是因为传感器片上固件决定了 PSK 和协议版本。

```bash
git clone --recurse-submodules https://github.com/mpi3d/goodix-fp-dump.git
cd goodix-fp-dump
python3 -m venv .venv && source .venv/bin/activate
pip install pyusb crcmod python-periphery spidev
sudo systemctl stop fprintd
python3 -c "import driver_55x4; driver_55x4.main(0x55a4)"
sudo systemctl start fprintd
```

一次性步骤完成后，PSK 自动补给机制处理后续所有场景。

## 使用

```bash
# 安装 fprintd
sudo apt install -y fprintd libpam-fprintd

# 录入指纹
fprintd-enroll -f right-index-finger $USER

# 验证
fprintd-verify $USER
#   verify-match     → 正确手指
#   verify-no-match  → 错误手指（预期行为）

# 启用系统级指纹认证（sudo / 登录 / 锁屏）
sudo pam-auth-update
# 勾选 "Fingerprint authentication" → 确认
```

## 安装内容

| 路径 | 用途 |
|------|------|
| `/usr/lib/x86_64-linux-gnu/libfprint-2.so*` | 含 goodixtls55x4 驱动的 libfprint |
| `/etc/udev/rules.d/99-goodix-fp.rules` | USB 设备权限（`0660 plugdev`） |
| `/usr/local/bin/goodix-psk-autofix` | PSK 检测与自动修复脚本 |
| `/etc/systemd/system/goodix-psk-autofix.service` | 在 fprintd 之前运行，校准 PSK |

## 双系统

Windows/Linux 切换会导致 PSK 被重写。本驱动自动处理：

```
启动 Linux
  → goodix-psk-autofix.service 运行
  → 读取传感器 PSK 哈希
  → 不匹配？写入社区密钥（~500ms）
  → fprintd 以有效 PSK 启动
```

无需用户干预。Windows 侧正常使用——启动时会重新写入自己的 PSK。

## 问题排查

### "failed to claim device"

有进程占用了设备，通常是 fprintd 本身。

```bash
sudo systemctl restart fprintd
```

### fprintd-list 看不到设备

检查 USB 设备可见性：

```bash
lsusb -d 27c6:
# 期望输出: "Shenzhen Goodix Technology Co.,Ltd. Goodix FingerPrint Device"
```

如果设备可见但 fprintd 不识别，udev 规则可能未加载：

```bash
sudo udevadm control --reload-rules && sudo udevadm trigger
```

### 录入指纹时卡住

部分硬件版本可能不触发手指按压中断。可以用 [goodix-fp-dump](https://github.com/mpi3d/goodix-fp-dump) 的 `run_55a4.py` 脚本验证传感器能正常采图。如果 Python 能出图但 fprintd 不行，传感器可能需要调整 FDT 阈值——请提 Issue。

### "Device reported an error: Cannot run while suspended"

传感器过热或超时。等待 30 秒重试即可。

## 修改说明

本驱动 fork 自 `TheWeirdDev/libfprint`（`55b4-experimental`），整合了 [PR #3](https://github.com/TheWeirdDev/libfprint/pull/3)（jedbillyb）的 14 个 commit：

- PSK 自动补给（不匹配时写入，匹配时跳过）
- PSK 写入帧格式修复（未初始化结构体 → 显式 payload）
- 固件版本前缀匹配（`GF3268_RTSEC_APP_*`）
- OpenSSL 3.x PSK 加密套件启用
- 多个命令的 payload 大小修复
- TLS 会话预热缓存
- sigfm 匹配阈值优化

加上我们的扩展：图像路由修复（cmd `0xd0` 接受）、udev 规则、PSK 自动修复服务、CI/CD 流水线、安全加固。

完整技术文档见 [TECHNICAL.md](TECHNICAL.md)。

## 安全

社区 PSK 是硬编码和公开的。TLS 通道提供**传输加密**（防止 USB 总线被动嗅探），但**不对主机进行身份认证**。指纹模板存储在 `/var/lib/fprint/`（0600）和传感器 MCU（加密通道）中。OpenSSL `@SECLEVEL=0` 配置仅作用于进程内传感器 socketpair，不影响系统其他 TLS 连接。USB 设备权限限制为 `plugdev` 组（`0660`），非全局可写。

详见 [SECURITY.md](SECURITY.md)。

## 许可

LGPL-2.1

## 致谢

本项目基于以下开源工作：

- [TheWeirdDev/libfprint](https://github.com/TheWeirdDev/libfprint) — goodixtls 驱动原作
- [jedbillyb/libfprint](https://github.com/jedbillyb/libfprint/tree/goodix-55b4-fixes) — PSK 补给、OpenSSL 修复、固件匹配
- [mpi3d/goodix-fp-dump](https://github.com/mpi3d/goodix-fp-dump) — 固件刷写与 PSK 配置工具
- [goodix-fp-linux-dev](https://github.com/goodix-fp-linux-dev) — 社区固件维护
