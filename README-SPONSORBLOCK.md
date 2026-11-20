# 修改版说明与编译指南

这份文档写给**想改代码、想自己编译**的人。如果只是想下载安装，请回到 [README.md](README.md) 看第一段。

## 1. 这是什么

基于 wiliwili v1.6.0（`yoga` 分支，提交 `88e5876`）的个人 fork，加入了空降助手（BilibiliSponsorBlock）的跳广告能力。

上游 [Issue #656](https://github.com/xfangfang/wiliwili/issues/656) 中维护者已明确表示不希望加入跳广告功能，所以本 fork **不会向原作者提交合并请求**。请尊重上游的决定。

## 2. 感谢

- 原作者 [xfangfang](https://github.com/xfangfang) 与 [wiliwili 原项目](https://github.com/xfangfang/wiliwili) —— 没有他们的开源工作就没有这个客户端。所有原始代码版权归原作者及原项目贡献者所有。若原作者不希望此修改版公开，我会立即删除本仓库。
- [hanydd/BilibiliSponsorBlock](https://github.com/hanydd/BilibiliSponsorBlock) 及 `bsbsb.top` 提供的标注数据与服务。
- `kurikomoe/MakeBLiveGreatAgain` 在进度条标注与跳过判定逻辑上的启发。

## 3. 功能怎么用

完全在播放器设置里操作，**不需要 FTP 连 Switch 改配置文件**：

1. 播放任意视频 → 打开播放器设置 → 找到「空降助手跳广告」（默认开启）。
2. 「空降助手详细设置」里可以改：
   - 服务器地址（默认 `https://www.bsbsb.top`），改完自动保存，无需重启；
   - 「测试服务器连接」，显示「连接正常 (HTTP 200)」即代表服务器可达；
   - 跳过策略：仅恰饭 / 恰饭+自我推广 / 全部可跳过片段；
   - 最短片段时长；
   - 进度条显示片段标识（开关）；
   - 显示跳过提示（开关）。

进度条颜色含义：橙色 = 恰饭（sponsor），蓝色 = 自我推广（selfpromo），紫色 = 片头，靛色 = 片尾，绿色 = 互动，粉色 =  filler/preview，灰色 = 其他。

测试视频：`BV14741127BN`，约 5:00 处有标注片段 `[300.019, 600.014]`，播放到该处会自动跳到 10:00。拖回 5:00 前等待 4 秒以上可再次自动跳过。

## 4. 工作原理（简单版）

- 播放时拿当前视频的 BVID + CID 请求空降助手 API 获取标注片段。
- 新接口：`GET /api/skipSegments/{sha256(bvid)前4位}`，按 `videoID` 精确匹配；兼容旧的 `GET /api/skipSegments?videoID=<bvid>&cid=<cid>`。
- 只处理 `actionType == "skip"` 的片段；默认跳过 `sponsor` 和 `selfpromo`。
- 进度命中 `[start - 0.3s, end)` 时 seek 到片段末尾，并显示提示。
- 同一片段有 4 秒冷却，避免反复跳转。
- 跳过时 `POST /api/viewedVideoSponsorTime` 上报一次观看，给标注者计数。
- 服务器测试使用 `GET /api/ready`，HTTP 200 即正常。

## 5. 想自己编译 Switch 版，需要准备什么

### 5.1 系统与基础工具

- Linux（本次实测在 Ubuntu/Debian 环境完成），磁盘建议 15 GB 以上。
- `git`、`curl`、`cmake`、`ninja`/`make`、`python3`、`pkg-config`、`pacman`（devkitPRO 用）、`tar`、`unzip`。

### 5.2 devkitPRO 工具链（核心）

需要安装到 `DEVKITPRO` 目录（不一定非要 `/opt/devkitpro`，本仓库用的是工作区内的便携目录）：

- `devkitA64`（实测 16.1.0）
- `libnx`（实测 4.12.0）
- `switch-tools`
- `switch-cmake`
- `deko3d`

### 5.3 wiliwili 的 Switch 依赖包

从 wiliwili 官方 release 获取（必须使用 deko3d 版本的 mpv）：

- `switch-ffmpeg`
- `switch-libmpv_deko3d`
- `switch-nspmini`
- `hacBrewPack`
- `libuam`

以及这些 portlibs（缺任何一个都会编译失败）：

`switch-curl`、`switch-glm`、`switch-libass`、`harfbuzz`、`fribidi`、`freetype`、`bzip2`、`libpng`、`libplacebo`、`liblua51`、`zlib`、`mbedtls`、`dav1d`、`libxml2`、`libjpeg-turbo`。

### 5.4 源码与子模块

本项目使用 GitCode 镜像克隆（GitHub 直连可能不稳定），并需要补齐子模块：

```bash
git clone https://gitcode.com/gh_mirrors/wi/wiliwili.git
git submodule update --init --recursive
```

若子模块下载失败，可从固定 commit 下载 tarball 放到 `library/` 对应目录。已知的必需子模块包括 `borealis`、`cpr`、`pystring`、`mongoose`、`libpdr`、`lunasvg`、`MemoryModule`、`OpenCC`、`QR-Code-generator`。

### 5.5 一处必要的兼容性补丁

Switch 用的 newlib 没有 `<sys/mman.h>`，导致 `library/lunasvg/plutovg/source/plutovg-font.c` 编译失败。仓库中保留了补丁文件 `patches-lunasvg-mmap-compat.patch`。核心改动是把 `mmap` 读字体改为条件编译：

```c
#if defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__)
#include <sys/mman.h>
#endif
```

其他平台改用 `fopen/fseek/ftell/fread/malloc/free` 读字体文件。

### 5.6 编译命令

```bash
export DEVKITPRO=/path/to/devkitpro
export DEVKITA64=$DEVKITPRO/devkitA64
export PATH=$DEVKITA64/bin:$DEVKITPRO/tools/bin:$DEVKITPRO/portlibs/switch/bin:$PATH
export PKG_CONFIG_LIBDIR=$DEVKITPRO/portlibs/switch/lib/pkgconfig
export PKG_CONFIG_PATH=$PKG_CONFIG_LIBDIR
export PKG_CONFIG_SYSROOT_DIR=/path/to/devkitpro-sysroot
export TMPDIR=/path/to/roomy-tmp   # 系统 /tmp 通常只有 10MB，不够编译用

cmake -B cmake-build-switch -DPLATFORM_SWITCH=ON -DUSE_DEKO3D=ON \
      -DBUILTIN_NSP=ON -DBRLS_UNITY_BUILD=ON
cmake --build cmake-build-switch -j4
```

产物：`cmake-build-switch/wiliwili.nro`

> 注意：`PKG_CONFIG_SYSROOT_DIR` 用于把 `.pc` 文件里硬编码的 `/opt/devkitpro` 重映射到你的实际工具链目录，否则会找不到库。

### 5.7 一键重建脚本（本机已搭好）

本次工作区已搭好便携式工具链，直接执行即可：

```bash
cd /home/mark/WorkBuddy/2026-11-20-15-37-28
./build_switch_local.sh
```

脚本内含全部环境变量、路径重映射、`TMPDIR` 设置和 `-j4` 并行度控制。

## 6. 安装到 Switch

1. 把 `wiliwili.nro` 复制到内存卡的 `switch/wiliwili/wiliwili.nro`。
2. 从 hbmenu 或主页图标启动。
3. 想保留原版：把原来的文件改名为 `wiliwili-original.nro` 共存。
4. 可选：`wiliwili-forwarder.nsp` 可用 Install 类工具安装，用来在主页生成图标；应用内也有「设置 → 实用工具 → 使用教程」说明。

## 7. 注意事项

- 需要 Switch 能访问 `bsbsb.top`；项目已有的代理/SSL/DNS 设置会复用。
- 覆盖率取决于社区标注，热门视频通常有数据，冷门视频可能没有。可以在浏览器装「小电视空降助手」自行标注。
- 口播广告是许多 UP 主的收入来源，建议对自己支持的 UP 主酌情关闭此功能。
