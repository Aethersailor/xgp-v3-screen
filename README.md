# XGP V3 Screen

面向 ImmortalWrt 的 NLnet XiGuaPi V3 板载 TFT 屏幕控制程序，提供 LuCI 配置界面。

## 项目来源

- **原始项目**由 zzzz0317 开发。[zzzz0317/xgp-v3-screen](https://github.com/zzzz0317/xgp-v3-screen)
- **带 LuCI 的社区版**是本项目的直接上游，移植以提交 `85b792d` 为基线。[junhong-l/xgp-v3-screen](https://github.com/junhong-l/xgp-v3-screen/tree/85b792d2dd224b8c45811ec260e580df51a499e7)
- **许可证** GPL-3.0

## 改造内容

### 移植显示后端到 ImmortalWrt

上游通过 framebuffer 访问 GC9307，依赖 `kmod-fb-tft-gc9307` 和 `kmod-fb-tft`。本项目将 LVGL 显示后端移植到 ImmortalWrt 用户态，使用官方固件提供的 `spidev`、GPIO 字符设备和 LED 背光接口。

屏幕服务启动时，根据板型和设备树识别 GC9307，将 `spi3.0` 绑定到 `spidev`。程序经 `/dev/spidev3.0` 写入屏幕命令和 RGB565 像素，通过 GPIO 控制命令与数据切换及复位，通过 LED 接口控制背光。当前 SPI 速率为 50 MHz，切页转场时长为 300 ms。

### 使用原生 LuCI 配置和 APK 打包

配置界面从上游的 Lua CBI 改为原生 LuCI JavaScript 表单，配套菜单、`rpcd` 访问权限和流量清零接口。屏幕服务、LuCI 配置页及翻译打包为 `luci-app-xgp-v3-screen`，使用 ImmortalWrt `rockchip/armv8` SDK 构建 APK。安装时由 ImmortalWrt 官方软件源提供用户态依赖。[依赖详情](#依赖)

### 更新网络和调制解调器信息

网络页通过 netifd 获取实际拨号设备和 LAN 的三层设备，用于显示 Modem IP、LAN IP 和统计 LAN 的 IPv4 ARP 条目。Modem IP 也支持按 `wwan*` 和 `rmnet_mhi*` 接口名前缀查找。

屏幕可从 `luci-app-fm350` 读取频段、温度、信号等信息，并通过只读 AT 命令查询电压和 AMBR。QModem 仍可提供调制解调器信息及其支持的 CQI 指标。

## 项目结构

```
xgp-v3-screen/
├── Makefile                    # ImmortalWrt 用户态软件包
├── kmod-fb-tft-gc9307/         # 社区版驱动参考源码
├── files/
│   ├── bind-panel.sh          # 检查板型并绑定 spidev
│   ├── modem_info.py          # Modem 信息获取脚本
│   ├── network_devices.sh     # 查询 Modem 和 LAN 对应的网络设备
│   ├── xgp_screen.config      # 默认 UCI 配置
│   ├── uci-defaults-xgp-screen # UCI 初始化脚本
│   └── zz_xgp_screen.init     # procd 服务脚本
├── htdocs/                     # LuCI JavaScript 配置页
├── po/                         # LuCI 翻译
├── root/                       # LuCI 菜单、rpcd 权限及流量清零接口
└── src/
    ├── main.c                  # 主程序
    ├── gc9307_spi.c/h          # LVGL SPI 显示后端
    ├── screen_config.c/h       # 配置解析模块
    ├── screen_manager.c/h      # 屏幕管理器
    └── ui/                     # LVGL UI 文件
```

## 编译安装

### 前置条件

使用 ImmortalWrt `rockchip/armv8` SDK。安装前确认固件提供屏幕服务所需的设备接口。[依赖详情](#依赖)

### 1. 放置源码
```bash
# 将项目放入 OpenWrt 的 package 目录
cp -r xgp-v3-screen ~/openwrt/package/luci-app-xgp-v3-screen
```

### 2. 选择包
```bash
make menuconfig
# 进入 Utilities，选择 luci-app-xgp-v3-screen
```

### 3. 编译
```bash
make package/luci-app-xgp-v3-screen/compile -j$(nproc)
```

编译生成 `aarch64_generic` 架构的 `luci-app-xgp-v3-screen`，同一个包包含屏幕服务和 LuCI 界面。

GitHub Actions 使用 ImmortalWrt `rockchip/armv8` Snapshot SDK。手动运行 **Build APK** 可下载构建产物。推送与 Makefile 版本一致的标签（例如 `v2.2-r8`）后，工作流会构建 APK，并将 APK 和 `SHA256SUMS` 发布到 GitHub Release。

### 4. 安装
如果设备正在运行此前部署的 Lua 服务 `xgp-lcd`，先记录它的启用状态，然后停止并禁用它。确认旧进程已退出后再安装本包，安装时系统会尝试启动新服务。

```bash
/etc/init.d/xgp-lcd stop
/etc/init.d/xgp-lcd disable
apk add --allow-untrusted ./luci-app-xgp-v3-screen-2.2-r8.apk
```

回退时，停止并禁用 `zz_xgp_screen`，确认其进程已退出，再按原启用状态启动 `xgp-lcd`。两个服务共用 `spidev` 绑定。

## 配置文件

在 LuCI 的「服务 → XGP 屏幕」中管理屏幕开关、页面轮播和流量统计。「保存」暂存配置，「保存并应用」提交配置并重载屏幕服务。单页停留时间填 `0` 时使用默认停留时间。

「帧率上限」可选 15、20、25 或 32 FPS，默认 32 FPS。实际帧率取决于切页效果和屏幕内容。

每页可单独设置启用状态、停留时间和顺序。通过 LuCI 停用屏幕时，屏幕服务和背光一并关闭。

### 亮屏方式

- **始终开启**是默认模式。开启屏幕后持续亮屏。
- **固定时段**按路由器的本地时间每天亮屏。开启时间和关闭时间采用 24 小时制。关闭时间早于开启时间时，将于次日关闭。
- **日出开启、日落关闭**由设备根据所在地经纬度计算。填写带正负号的十进制度数。 [在 LatLong.net 查询经纬度](https://www.latlong.net/)

开机或应用设置时，屏幕服务按当前时间判断是否亮屏。计划熄屏期间暂停刷新和屏幕数据采集。

`skip_boot` 和 `skip_splash` 保留在 UCI 中供兼容。当前屏幕程序直接进入轮播。

配置文件位置: `/etc/config/xgp_screen`

```
config screen 'main'
    option enabled '1'
    option schedule_mode 'always'
    option max_fps '32'
    option scroll_type 'slide_up'
    option default_delay '5000'
    option skip_boot '0'
    option skip_splash '0'

config traffic 'settings'
    list interfaces 'eth1'
    list interfaces 'wwan0'
    list interfaces 'wwan0_1'
    option reset '0'

config page 'system_info'
    option enabled '1'
    option delay '5000'
    option order '1'

config page 'system_status'
    option enabled '1'
    option delay '5000'
    option order '2'

config page 'traffic'
    option enabled '1'
    option delay '5000'
    option order '6'

# ... 更多页面配置
```

### 切页动画

在 LuCI 中选择切换效果，或通过 UCI 的 `scroll_type` 设置。

| 配置值 | 效果 |
| --- | --- |
| `none` | 无动画 |
| `fade` | 淡入 |
| `slide_up` | 向上滑动（默认） |
| `slide_down` | 向下滑动 |
| `slide_left` | 向左滑动 |
| `slide_right` | 向右滑动 |
| `over_up` | 从下往上覆盖 |
| `over_down` | 从上往下覆盖 |
| `over_left` | 从右往左覆盖 |
| `over_right` | 从左往右覆盖 |

### 流量统计功能

统计页汇总所配置接口的上传和下载流量。接口列表支持按名称或前缀匹配，统计值根据大小在 B、KB、MB、GB 之间切换。

**支持的接口类型：**
- 有线WAN: `eth1`
- 移动网络: `wwan0`, `wwan0_1`, `wwan1` 等
- PPPoE拨号: `pppoe-wan`
- 通用WAN: `wan`, `wan6`

**配置示例：**
```bash
# 添加监控接口
uci add_list xgp_screen.settings.interfaces='eth1'
uci add_list xgp_screen.settings.interfaces='wwan'
uci commit xgp_screen

# 手动清零流量统计
uci set xgp_screen.settings.reset='1'
uci commit xgp_screen
/etc/init.d/zz_xgp_screen restart

# 屏幕下次显示流量统计页时清零，标志自动恢复为 0
```

**数据存储：**
- 位置: `/etc/xgp_screen_traffic.dat`
- 格式: 文本格式，包含总流量和当日流量
- 更新频率: 每2秒
- 重置时间: 每日00:00自动重置当日统计
## 依赖

- **固件提供的设备接口**包括 `spidev`、GPIO 字符设备和板载背光接口。当前包的设备检查针对 `nlnet,xiguapi-v3` 及设备树中的 `galaxycore,gc9307`。
- **官方软件源提供的运行时依赖**包括执行 Modem 采集脚本的 `python3`、多线程运行库 `libpthread`、C++ 标准库 `libstdcpp` 和 LuCI 基础包 `luci-base`。
- **构建依赖**包括 ImmortalWrt `rockchip/armv8` SDK 和用于编译 LuCI 翻译的 `luci-base/host`。
- **可选调制解调器数据源**为 `luci-app-fm350` 或 QModem。设备安装了相应管理插件后，屏幕即可读取其提供的调制解调器信息。

## 服务管理

```bash
# 启动
/etc/init.d/zz_xgp_screen start

# 停止
/etc/init.d/zz_xgp_screen stop

# 重启
/etc/init.d/zz_xgp_screen restart

# 查看状态
ps | grep zz_xgp_screen
```

## 许可证

GPL-3.0-only

## 致谢

- 原始项目作者 [zzzz0317](https://github.com/zzzz0317)
  - [xgp-v3-screen](https://github.com/zzzz0317/xgp-v3-screen) - 屏幕控制程序
  - [kmod-fb-tft-gc9307](https://github.com/zzzz0317/kmod-fb-tft-gc9307) - GC9307 驱动
- [LVGL](https://lvgl.io/) - 嵌入式图形库
