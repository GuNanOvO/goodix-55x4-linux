# Goodix GF3268/GF3208 (27c6:55a4) 逆向记录

## 硬件

- 芯片: Goodix GF3208 / GF3268 (Taobao 指纹模块)
- USB VID:PID: `27c6:55a4`
- 接口: 1× Vendor Specific (0xff), 2× Bulk EP (0x01 OUT, 0x82 IN)
- IAP 引导: `MILAN_RTSEC_IAP_10027`
- 架构: TLS (Touch-on-Host via mbedTLS PSK)

## Windows 驱动 (Lenovo GoodixFPT v3.1.596.517)

- 封装: InnoSetup → `innoextract` 解包
- 主 DLL: `Wbdi.dll` (2.3MB, PE32+)
  - 内含 10+ 种汇顶芯片固件
  - 固件变体: GM168SEC (SGX), RTSEC, HT (Host Touch)
  - 自动刷写: `firmware version not equal, update firmware app!!!`

### PSK 机制
- Windows 使用 Intel SGX 动态生成 PSK（非硬编码）
- 生成后通过 `preset_psk_write` 写入 MCU
- 加密套件: `TLS-PSK-WITH-AES-128-CBC-SHA256`
- 社区 PSK (PMK_HASH): `81b8ff490612022a121a9449ee3aad2792f32b9f3141182cd01019945ee50361`
  - 存储在 `driver_55x4.py` 和 `goodix55x4.h`

### 固件版本
- 设备原始 (Windows): `GF3208_RTSEC_APP_10062`
- 社区目标: `GF3268_RTSEC_APP_10041` (goodix-fw repo 子模块)
- 固件备份: goodix-fp-dump 刷写前自动备份

## 社区驱动 (TheWeirdDev/libfprint, 55b4-experimental)

### 文件结构
```
goodixtls/
├── goodixtls.c/h      # TLS 代理 (OpenSSL PSK server)
├── goodix.c/h         # 协议层 (USB 命令收发)
├── goodix55x4.c/h     # 55x4 驱动 (GF3268/GF3208)
├── goodix511.c/h      # 511x 驱动
└── goodix_proto.c/h   # USB 协议编解码
```

### 修改项 (v0.1.0)

1. **固件版本检查放宽** (`goodix55x4.c:109`)
   - 原: `strcmp(firmware, "GF3268_RTSEC_APP_10041")`
   - 改: `g_str_has_prefix(firmware, "GF32")`
   - 原因: 设备可能是 GF3208 或 GF3268

2. **命令 payload 修复** (`goodix.c:1083,1134,1160,1185`)
   - 原: `GoodixNone payload = {}` (未定义结构体)
   - 改: `guint8 payload[2] = {0, 0}`
   - 原因: Python 驱动所有命令 payload 均为 2 字节

3. **TLS 图像路由** (`goodix.c:350`)
   - 新增: 命令 0xd0 作为 0x20 的回复被接受
   - 原因: 固件 10041 在取图时通过 TLS 通道返回 0xd0

### USB 命令码 (goodix_proto.h)

| 命令 | 值 | 说明 |
|------|-----|------|
| NOP | 0x00 | 空操作 |
| MCU_GET_IMAGE | 0x20 | 获取指纹图像 |
| FDT_DOWN | 0x32 | 指纹检测降沿 |
| FDT_UP | 0x34 | 指纹检测升沿 |
| FDT_MODE | 0x36 | 指纹检测模式 |
| SLEEP_MODE | 0x60 | 休眠 |
| IDLE_MODE | 0x70 | 空闲 |
| SENSOR_REG_WRITE | 0x80 | 写传感器寄存器 |
| SENSOR_REG_READ | 0x82 | 读传感器寄存器 |
| UPLOAD_CONFIG | 0x90 | 上传 MCU 配置 |
| SLEEP_REALTEK | 0x92 | Realtek 休眠 |
| ENABLE_CHIP | 0x96 | 芯片使能 |
| RESET | 0xa2 | 复位 |
| ERASE_APP | 0xa4 | 擦除应用固件 |
| READ_OTP | 0xa6 | 读 OTP |
| FIRMWARE_VERSION | 0xa8 | 读固件版本 |
| QUERY_MCU_STATE | 0xae | 查询 MCU 状态 |
| ACK | 0xb0 | 确认 |
| SET_DRV_STATE | 0xc4 | 设置驱动状态 |
| REQUEST_TLS | 0xd0 | 请求 TLS 连接 |
| GET_POV_IMAGE | 0xd2 | 获取 POV 图像 |
| TLS_ESTABLISHED | 0xd4 | TLS 连接确认 |
| PSK_WRITE | 0xe0 | 写 PSK |
| PSK_READ | 0xe4 | 读 PSK |
| WRITE_FIRMWARE | 0xf0 | 写固件 |
| READ_FIRMWARE | 0xf2 | 读固件 |
| CHECK_FIRMWARE | 0xf4 | 校验固件 |
| GET_IAP_VERSION | 0xf6 | 读 IAP 版本 |

### 配置

- PSK (PMK_HASH): 32 字节 (见 `goodix55x4.h`)
- PSK 白盒: 96 字节 (见 `driver_55x4.py`)
- 设备配置: ~250 字节 (见 `goodix55x4.h`)
- 传感器: 88×108 像素

## 刷固件流程

1. 安装 goodix-fp-dump 依赖 (Python venv)
2. 配置 udev 规则: `SUBSYSTEM=="usb", ATTRS{idVendor}=="27c6", ATTRS{idProduct}=="55a4", MODE="0666"`
3. 停止 fprintd: `sudo systemctl stop fprintd`
4. 运行刷写: `python3 -c "import driver_55x4; driver_55x4.main(0x55a4)"`
5. 固件自动备份 → 擦除 → 刷写 GF3268_RTSEC_APP_10041
6. 重建 C 驱动
