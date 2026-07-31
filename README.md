# Goodix GF32xx Fingerprint Driver for Linux

为 Goodix GF3208/GF3268 (55x4 系列) 指纹传感器提供 Linux 驱动支持。

## 适用硬件

| USB ID | 芯片 | 状态 |
|--------|------|------|
| `27c6:55a4` | GF3208 / GF3268 | ✅ 已测试 |
| `27c6:55b4` | GF3268 | ✅ 驱动已支持 |
| `27c6:5584` | GF3268 | 🔧 可扩展 |

## 工作流程

```
Windows 驱动 (一次性) → goodixtls (Linux) → fprintd 可用
```

驱动内置 PSK 自动修复——如果设备之前被 Windows 驱动写过 PSK，驱动启动时自动校准。

### 一键安装

```bash
# 1. 下载最新 Release
wget https://github.com/GuNanOvO/goodix-55x4-linux/releases/latest/download/goodix-gf32xx-driver_*.deb

# 2. 安装
sudo dpkg -i goodix-gf32xx-driver_*.deb
sudo apt install --fix-broken -y

# 3. 安装 udev 规则 + PSK 自动修复服务
sudo ./tools/install.sh

# 4. 如果固件仍是 Windows 版本，刷入社区固件 (仅需一次)
# git clone --recurse-submodules https://github.com/mpi3d/goodix-fp-dump.git
# cd goodix-fp-dump && python3 -m venv .venv && source .venv/bin/activate
# pip install pyusb crcmod && python3 -c "import driver_55x4; driver_55x4.main(0x55a4)"
```

### 编译安装 (从源码)

```bash
git clone --recursive https://github.com/GuNanOvO/goodix-55x4-linux.git
cd goodix-55x4-linux
sudo ./tools/install.sh
```

### 使用

```bash
# 安装 fprintd
sudo apt install -y fprintd libpam-fprintd

# 录入指纹
fprintd-enroll -f right-index-finger $USER

# 验证
fprintd-verify $USER

# 启用系统认证 (sudo/login)
sudo pam-auth-update  # 勾选 "Fingerprint authentication"
```

## 修改说明

基于 `TheWeirdDev/libfprint` (`55b4-experimental` 分支) 的 goodixtls 驱动:

1. 固件版本检查 → 前缀匹配 (GF32\*)
2. 命令 payload → 修复为 2 字节
3. TLS 图像路由 → 接受 cmd=0xd0 作为图像就绪信号

详见 [RE_NOTES.md](docs/RE_NOTES.md)

## 许可

LGPL-2.1
