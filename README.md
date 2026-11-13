# XGP V3 Screen

NLnet XiGuaPi V3 TFT 屏幕控制程序，支持 Web 配置界面。

此版本以 [带 LuCI 的社区版](https://github.com/junhong-l/xgp-v3-screen/tree/85b792d2dd224b8c45811ec260e580df51a499e7) 为基线，通过内建 `spidev` 驱动板载 GC9307。

## 项目来源

- **原始项目**: [zzzz0317/xgp-v3-screen](https://github.com/zzzz0317/xgp-v3-screen)
- **原始作者**: zzzz0317
- **许可证**: GPL-3.0

## 改造内容

本项目在原始版本基础上进行了以下改造：

### 1. 用户态屏幕驱动
- 通过 `/dev/spidev3.0` 和 GPIO 字符设备发送 GC9307 命令及局部刷新数据。
- 使用板载 LED 接口控制背光。

### 2. 新增 LuCI Web 控制界面
- ✅ 集成到 OpenWrt 管理界面（服务 → XGP Screen）
- ✅ 支持配置项：
  - 启用/禁用屏幕（同时控制背光）
  - 切换动画类型（10种动画效果）
  - 默认停留时间
  - 跳过启动/闪屏选项沿用社区版。当前社区版入口直接开始状态页轮播。
  - 每个页面独立配置（启用、停留时间、显示顺序）
  - 流量监控接口配置和手动清零

### 3. 屏幕切换动画
支持以下动画效果：
| 配置值 | 效果 |
|-------|------|
| `none` | 无动画 |
| `fade` | 淡入淡出 |
| `slide_up` | 向上滑动（默认） |
| `slide_down` | 向下滑动 |
| `slide_left` | 向左滑动 |
| `slide_right` | 向右滑动 |
| `over_up` | 从下往上覆盖 |
| `over_down` | 从上往下覆盖 |
| `over_left` | 从右往左覆盖 |
| `over_right` | 从左往右覆盖 |

### 4. 修复 Modem IP 显示问题
- ✅ 支持 `wwan*` 和 `rmnet_mhi*` 两种接口类型

### 5. 屏幕管理
- ✅ 移除硬编码的屏幕切换逻辑
- ✅ 由配置文件统一管理屏幕切换
- ✅ 关闭屏幕时自动关闭背光

### 6. 流量监控页面
- ✅ 实时显示WAN接口上传/下载流量统计
- ✅ 支持自定义监控接口列表（eth1, wwan, wan, pppoe等）
- ✅ 支持前缀匹配，灵活配置移动网络接口
- ✅ 自动累加多个WAN接口流量（支持双WAN场景）
- ✅ 数据持久化存储（`/etc/xgp_screen_traffic.dat`）
- ✅ 每日零点自动重置当日统计
- ✅ 手动清零功能（LuCI界面或命令行）
- ✅ 智能单位转换（B/KB/MB/GB自动选择）
- ✅ 每2秒自动刷新数据

## 项目结构

```
xgp-v3-screen/
├── Makefile                    # OpenWrt 用户态软件包
├── kmod-fb-tft-gc9307/         # 社区版驱动参考源码
├── files/
│   ├── bind-panel.sh          # 检查板型并绑定 spidev
│   ├── modem_info.py          # Modem 信息获取脚本
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

使用 ImmortalWrt `rockchip/armv8` SDK。设备需要内建 `spidev`、GPIO 字符设备和板载背光接口。

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

GitHub Actions 使用 ImmortalWrt `rockchip/armv8` Snapshot SDK。手动运行 **Build APK** 可下载构建产物。推送与 Makefile 版本一致的标签（例如 `v2.2-r3`）后，工作流会将 APK 和 `SHA256SUMS` 上传到 GitHub Release。

### 4. 安装
如果设备正在运行此前部署的 Lua 服务 `xgp-lcd`，先记录它的启用状态，然后停止并禁用它。确认旧进程已退出后再安装本包，安装时系统会尝试启动新服务。

```bash
/etc/init.d/xgp-lcd stop
/etc/init.d/xgp-lcd disable
apk add --allow-untrusted ./luci-app-xgp-v3-screen-2.2-r3.apk
```

回退时，停止并禁用 `zz_xgp_screen`，确认其进程已退出，再按原启用状态启动 `xgp-lcd`。两个服务共用 `spidev` 绑定。

## 配置文件

在 LuCI 的「服务 → XGP 屏幕」中管理屏幕开关、页面轮播和流量统计。「保存」暂存配置，「保存并应用」提交配置并重载屏幕服务。单页停留时间填 `0` 时使用默认停留时间。

`skip_boot` 和 `skip_splash` 保留在 UCI 中供兼容。当前屏幕程序直接进入轮播。

配置文件位置: `/etc/config/xgp_screen`

```
config screen 'main'
    option enabled '1'
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

### 流量统计功能

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

**编译依赖：**
- OpenWrt 编译环境

**运行时依赖：**
- `python3` - Modem 信息获取
- `libpthread` - 多线程支持
- `libstdcpp` - C++ 标准库
- `luci-base` - LuCI 基础框架

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
