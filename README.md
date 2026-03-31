# my-app — BLE RGB LED Demo

通过 **BLE（低功耗蓝牙）** 控制 Seeed XIAO BLE (nRF52840) 板载 RGB LED 的亮灭。

固件实现了一个自定义 GATT 服务，提供 **3 个写特征值**（分别控制红/绿/蓝）和 **1 个 Notify 特征值**（状态变化时主动推送）。配套一个纯静态 HTML 页面，使用浏览器内置的 **Web Bluetooth API** 进行控制，无需安装任何 App。

## 硬件

XIAO BLE 板载 RGB LED（低电平有效）：

| 颜色 | 设备树别名 | 引脚  |
| ---- | ---------- | ----- |
| 红   | led0       | P0.26 |
| 绿   | led1       | P0.30 |
| 蓝   | led2       | P0.06 |

## BLE GATT 服务

广播设备名：`RGB LED BLE Demo`

服务基础 UUID：`12340000-0000-1000-8000-00805F9B34FB`

| 特征值 | UUID（末段） | 权限   | 数据格式          | 说明                   |
| ------ | ------------ | ------ | ----------------- | ---------------------- |
| Red    | `...0001`    | Write  | `0x00` / `0x01`   | 熄灭 / 点亮红色 LED    |
| Green  | `...0002`    | Write  | `0x00` / `0x01`   | 熄灭 / 点亮绿色 LED    |
| Blue   | `...0003`    | Write  | `0x00` / `0x01`   | 熄灭 / 点亮蓝色 LED    |
| Status | `...0004`    | Notify | `[R, G, B]` 3字节 | LED 状态变化时自动推送 |

## 项目结构

```
my-app/
├── west.yml              # West manifest（声明 Zephyr 依赖）
├── CMakeLists.txt        # 构建脚本
├── prj.conf              # 项目 Kconfig 配置（含 BLE 协议栈）
├── README.md             # 本文件
├── ble_control.html      # Web Bluetooth 控制页面（静态 HTML）
├── boards/
│   └── xiao_ble.conf     # 板级 Kconfig 覆盖配置
└── src/
    └── main.c            # BLE GATT 服务 + LED 控制固件
```

## 从零开始

### 1. 搭建 Zephyr 环境

```bash
# 安装 west
pip install west

# 用本仓库的 west.yml 作为 manifest 初始化工作区
west init -m https://github.com/YOUR_USERNAME/my-app.git my-workspace
cd my-workspace

# 拉取所有依赖（Zephyr 内核 + 模块）
west update

# 导出 Zephyr CMake 包（首次需要）
west zephyr-export

# 创建 Python 虚拟环境并安装依赖
python3 -m venv .venv
source .venv/bin/activate
pip install -r zephyr/scripts/requirements.txt
```

### 2. 构建

```bash
west build -p always -b xiao_ble my-app
```

### 3. 烧录

双击 XIAO BLE 的 RESET 按钮进入 UF2 引导模式，然后执行：

```bash
west flash -r uf2
```

或手动复制 UF2 文件：

```bash
cp build/zephyr/zephyr.uf2 /Volumes/XIAO-SENS/
```

烧录成功后串口会输出：

```
=== BLE RGB LED Demo ===
[BLE] 广播已开始，设备名: "RGB LED BLE Demo"
[BLE] 等待手机连接...
```

## 使用 Web 控制页面

`ble_control.html` 是一个零依赖的静态 HTML 文件，通过浏览器内置 Web Bluetooth API 连接设备。

> **要求：** Chrome / Edge 桌面版，且页面须通过 **HTTPS 或 localhost** 访问（file:// 不支持 Web Bluetooth）。

### 本地启动

```bash
# 在 my-app 目录下启动简易 HTTP 服务
python3 -m http.server 8765 --directory my-app
```

用 Chrome 打开 `http://localhost:8765/ble_control.html`，点击「扫描并连接设备」即可。

### 操作说明

1. 点击 **「扫描并连接设备」** → 浏览器弹出 BLE 扫描窗口，选择 `RGB LED BLE Demo`
2. 连接成功后，点击红/绿/蓝三张卡片切换对应 LED 亮灭
3. 底部 **Notify 日志**实时显示设备推送的 `[R, G, B]` 状态（每次写入后触发）
4. 点击「⛔ 断开连接」或直接关闭页面即可断开；断开后设备自动重新开始广播

## 引脚映射（设备树）

LED 到引脚的映射定义在 Zephyr 板级设备树文件
`zephyr/boards/seeed/xiao_ble/xiao_ble_common.dtsi` 中。
如需为自制板修改引脚映射，在项目中添加 `boards/xiao_ble.overlay` 文件覆盖即可。

## 一些资料

- https://docs.zephyrproject.org/latest/boards/seeed/xiao_ble/doc/index.html
- https://wiki.seeedstudio.com/cn/XIAO_BLE/
- https://www.nologo.tech/product/keyboard/keyboard_controll/super52840/super528401.html
