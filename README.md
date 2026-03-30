# my-app

Seeed XIAO BLE (nRF52840) 板载 RGB LED 三色换色示例。

循环显示 8 种颜色：红 → 绿 → 蓝 → 黄 → 青 → 品红 → 白 → 灭，每步 500ms，循环周期 4 秒。

## 硬件

XIAO BLE 板载 RGB LED（低电平有效）：

| 颜色 | 设备树别名 | 引脚   |
|------|-----------|--------|
| 红   | led0      | P0.26  |
| 绿   | led1      | P0.30  |
| 蓝   | led2      | P0.06  |

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

## 项目结构

```
my-app/
├── west.yml            # West manifest（声明 Zephyr 依赖）
├── CMakeLists.txt      # 构建脚本
├── prj.conf            # 项目 Kconfig 配置
├── .gitignore          # Git 忽略规则
├── README.md           # 本文件
├── boards/
│   └── xiao_ble.conf   # 板级 Kconfig 覆盖配置
└── src/
    └── main.c          # RGB LED 三色换色源码
```

## 引脚映射（设备树）

LED 到引脚的映射定义在 Zephyr 板级设备树文件
`zephyr/boards/seeed/xiao_ble/xiao_ble_common.dtsi` 中。
如果需要为自制板修改引脚映射，在项目中添加 `boards/xiao_ble.overlay` 文件即可覆盖。
