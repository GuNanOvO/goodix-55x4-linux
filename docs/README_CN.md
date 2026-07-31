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
Windows 驱动 (一次性) → 写 PSK → 刷固件 → goodixtls (Linux) → fprintd 可用
```

### 前置条件 (刷固件，仅需一次)

```bash
# 1. 克隆工具
git clone --recurse-submodules https://github.com/mpi3d/goodix-fp-dump.git
cd goodix-fp-dump

# 2. 安装 Python 依赖
python3 -m venv .venv && source .venv/bin/activate
pip install pyusb crcmod python-periphery spidev

# 3. 配置 USB 权限
echo 'SUBSYSTEM=="usb", ATTRS{idVendor}=="27c6", ATTRS{idProduct}=="55a4", MODE="0666"' | sudo tee /etc/udev/rules.d/99-goodix.rules
sudo udevadm control --reload-rules && sudo udevadm trigger

# 4. 停止 fprintd
sudo systemctl stop fprintd

# 5. 写 PSK + 刷固件 (GF3268_RTSEC_APP_10041)
python3 -c "import driver_55x4; driver_55x4.main(0x55a4)"
```

### 编译安装驱动

```bash
# 依赖
sudo apt install -y meson ninja-build libfprint-2-dev libglib2.0-dev libgusb-dev \
  libnss3-dev libssl-dev libcairo2-dev gobject-introspection libgirepository1.0-dev \
  libopencv-dev

# 构建
meson setup builddir --prefix=/usr/local -Ddrivers=goodixtls55x4
ninja -C builddir
sudo ninja -C builddir install
sudo ldconfig
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
