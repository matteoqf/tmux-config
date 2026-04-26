# tmux-config

高性能 tmux 配置 + 莫兰迪配色状态栏，适用于 macOS Apple Silicon。

## 文件结构

```
.
├── .tmux.conf          # tmux 主配置文件
├── Makefile            # 编译 + 安装
├── bin/                # 编译产物 (C 程序)
├── scripts/            # Shell 辅助脚本
└── src/                # C 源码
```

## 各文件功能

### `.tmux.conf`
tmux 主配置文件。

| 配置项 | 说明 |
|--------|------|
| 快捷键前缀 | `Ctrl+a` |
| `v` / `h` | 垂直/水平分屏 |
| `Ctrl+h/j/k/l` | Vim 风格分屏导航 |
| 鼠标模式 | 开启，拖拽复制自动推送 pbcopy |
| 状态栏 | C 程序驱动，2 秒刷新 |

安装: `cp .tmux.conf ~/.tmux.conf`

---

### `bin/tmux_status_daemon` (C)

高性能状态栏数据采集守护进程，macOS Apple Silicon 专用。

- 常驻内存，每 2 秒采集一次系统数据
- 写入 POSIX 共享内存 `/tmux_status`
- 无进程启动开销
- 自动守护自身，检测到数据过期自动重启

**依赖**: macOS Mach API、semaphores、shared memory

**编译**: `clang -O2 -o tmux_status_daemon src/tmux_status_daemon.c -lm`

---

### `bin/tmux_status_read` (C)

`tmux_status_daemon` 的读取端，专为 `tmux #()` 调用设计。

- 从共享内存读取数据，零进程启动开销
- 渲染五段 Powerline 风格状态栏:
  - CPU (🍎+使用率)、RAM (💻+使用率)
  - 网络 (↓↑ 带宽)、天气 (图标+温度)
  - 时间 (24小时制)

**编译**: `clang -O2 -o tmux_status_read src/tmux_status_read.c`

---

### `bin/tmux_left_bin` (C)

Powerline 风格左状态栏生成器。

- 输入: session 名、window 名、prefix 标记
- 输出: `[session] ` 格式左状态栏
- prefix 模式下背景变为 `#FFEE00` 高亮

**编译**: `clang -O2 -o tmux_left_bin src/tmux_left.c -lm`

---

### `scripts/tmux-cpu-ram.sh`

Shell 脚本，读取 CPU 和 RAM 使用率（备用方案）。

```bash
CPU=$(top -l 1 | grep "CPU usage" | awk '{print $3}' | tr -d '%')
# RAM: vm_stat + hw.memsize 计算已用页
echo "CPU:${CPU}% RAM:${RAM_PCT}%"
```

---

### `scripts/tmux-time-cn.sh`

中文日期时间脚本（中文 24 小时制）。

```bash
LC_TIME=zh_CN.UTF-8 date '+%A %m/%d %H:%M'
```

---

### `scripts/tmux-time.sh`

中文日期时间脚本（固定格式，含工作日）。

```bash
LC_TIME=zh_CN.UTF-8 date "+周三 %m/%d %H:%M:%S    "
```

---

## 快速安装

```bash
git clone https://github.com/YOUR_USER/tmux-config.git
cd tmux-config
make install
```

安装后 `~/.local/bin/` 包含所有 bin 和 scripts，`~/.tmux.conf` 已复制到 HOME。

## 依赖

- macOS (Apple Silicon)
- clang (编译)
- tmux 3.6+
- FiraCode Nerd Font (状态栏图标)
