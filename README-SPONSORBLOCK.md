# wiliwili（空降助手跳广告 Fork 版）

> 这是基于 [xfangfang/wiliwili](https://github.com/xfangfang/wiliwili) v1.6.0（`yoga` 分支，提交 `88e5876`）的**个人 fork**，增加了空降助手（BilibiliSponsorBlock）跳广告能力。
>
> 上游 [Issue #656](https://github.com/xfangfang/wiliwili/issues/656) 中维护者已明确表示不希望加入跳过广告功能（广告是 UP 主的收入来源），因此**本 fork 不会向原作者提交 PR / 合并请求**。请尊重上游维护者的决定。

## 致谢与声明

本仓库是个人修改版本，**特别感谢原作者 xfangfang 及 [wiliwili 原项目](https://github.com/xfangfang/wiliwili)** —— 正是基于他们优秀的开源工作，才有了这个客户端以及本 fork 的全部基础。本 fork 仅用于个人学习与自定义用途，所有原始代码版权归原作者及原项目贡献者所有。若原作者不希望此修改版公开，我会立即删除本仓库。

同时感谢：
- [hanydd/BilibiliSponsorBlock](https://github.com/hanydd/BilibiliSponsorBlock) 与 `bsbsb.top` 提供的空降助手标注数据与服务
- 参考项目 `kurikomoe/MakeBLiveGreatAgain` 在进度条标注与跳过判定逻辑上的启发

## 本 Fork 新增的功能

1. **自动跳过恰饭片段**：播放视频/番剧时，用当前 BVID + CID 请求空降助手 API，命中广告区间自动 seek 到片段末尾并弹出 OSD 提示。
2. **详细的图形化设置**：全部在播放器设置弹窗内完成，**无需 FTP 修改配置文件**：
   - 服务器地址（可自定义，默认 `https://www.bsbsb.top`）
   - 测试服务器连接（一键验证，显示 HTTP 状态码）
   - 跳过策略：仅恰饭 / 恰饭+自我推广 / 全部可跳过片段
   - 最短片段时长
   - 进度条显示片段标识（开关）
   - 显示跳过提示（开关）
3. **进度条彩色标识**：在播放器进度条上按片段类别叠加彩色标记（恰饭=橙、自我推广=蓝、片头=紫、片尾=靛、互动=绿等）。
4. **可重复跳转**：同一片段改为 4 秒冷却，手动拖回广告前等待数秒后可再次自动跳过（旧版用永久集合记录 UUID，导致只能跳一次）。
5. **提前量命中**：判定区间为 `[start - 0.3s, end)`，避免临界点漏跳。
6. **多语言**：新增文案覆盖简体中文、繁体中文、英文、日文、日文（RYU）、韩文、意大利文。

## API 与工作原理

- 获取片段：`GET https://www.bsbsb.top/api/skipSegments?videoID=<BVID>&cid=<CID>`
- 兼容新版前缀协议：`GET /api/skipSegments/{sha256(bvid)前4位}`，按 `videoID` 精确匹配
- 服务器测试：`GET /api/ready`，HTTP 200 即为正常
- 观看上报：`POST /api/viewedVideoSponsorTime`（给标注者计数）
- 默认处理 `sponsor` 与 `selfpromo`，且仅处理 `actionType == "skip"` 的片段

## 真机验证建议

- 测试视频：`BV14741127BN`，约 5:00 处有 sponsor 片段 `[300.019, 600.014]`
- 在播放器设置中点击「测试服务器连接」，应显示「连接正常」
- 播放到 5:00 附近应自动跳到 10:00，并显示跳过提示
- 拖回 5:00 前等待 4 秒以上，应能再次自动跳过
- 进度条上应能看到对应彩色标识

## 编译 Switch 版（NRO）

使用 devkitPRO / devkitA64 / libnx / deko3d：

```bash
export DEVKITPRO=/path/to/devkitpro
export DEVKITA64=$DEVKITPRO/devkitA64
export PATH=$DEVKITA64/bin:$DEVKITPRO/tools/bin:$DEVKITPRO/portlibs/switch/bin:$PATH
export PKG_CONFIG_LIBDIR=$DEVKITPRO/portlibs/switch/lib/pkgconfig
export PKG_CONFIG_SYSROOT_DIR=$DEVKITPRO

cmake -B cmake-build-switch -DPLATFORM_SWITCH=ON -DUSE_DEKO3D=ON \
      -DBUILTIN_NSP=ON -DBRLS_UNITY_BUILD=ON
cmake --build cmake-build-switch -j4
```

产物：`cmake-build-switch/wiliwili.nro`

> 注意：系统 `/tmp` 空间不足时，需设置 `TMPDIR` 到空间充足的目录。newlib 无 `<sys/mman.h>`，`library/lunasvg/plutovg/source/plutovg-font.c` 已打兼容性补丁。

## 安装到 Switch

1. 将 `wiliwili.nro` 复制到 microSD 卡的 `switch/wiliwili/wiliwili.nro`
2. 通过 hbmenu 或桌面转发器图标启动
3. 原版可备份为 `wiliwili-original.nro` 共存

## 致谢

- 原项目：[xfangfang/wiliwili](https://github.com/xfangfang/wiliwili)
- 空降助手：[hanydd/BilibiliSponsorBlock](https://github.com/hanydd/BilibiliSponsorBlock) 及 `bsbsb.top` 服务
- 参考实现：`kurikomoe/MakeBLiveGreatAgain` 的进度条标注与跳过判定逻辑
