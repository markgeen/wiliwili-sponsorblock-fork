//
// SponsorBlock (空降助手) integration for wiliwili
//

#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

struct SponsorSegment {
    double start = 0;
    double end   = 0;
    std::string uuid;
    std::string category;
};

/// 供进度条绘制使用的区间标记（单位：秒）
struct SponsorMark {
    double start = 0;
    double end   = 0;
    std::string category;
};

class SponsorBlockHelper {
public:
    static SponsorBlockHelper& instance() {
        static SponsorBlockHelper instance;
        return instance;
    }

    /// 总开关（由配置系统同步）
    inline static bool ENABLED = true;

    /// 空降助手服务器地址（设置页可改）
    inline static std::string SERVER = "https://www.bsbsb.top";

    /// 在进度条上按分类绘制片段标记
    inline static bool PROGRESS_MARK = true;

    /// 跳过时显示 OSD 提示
    inline static bool SHOW_TOAST = true;

    /// 忽略短于此时长的片段（秒）
    inline static double MIN_DURATION = 0;

    /// 跳过策略：0=仅赞助，1=赞助+自我推广（默认），2=全部可跳过片段
    inline static int MODE = 1;

    /// 当前视频切换时调用：记录 BVID/CID，清空旧片段并异步拉取新片段
    void setVideo(const std::string& bvid, uint64_t cid);

    /// 清空当前片段（停止播放或退出播放器时调用）
    void clear();

    /// 在播放进度回调中调用：命中广告区间则跳转到片段末尾
    void checkAndSkip();

    /// 测试服务器连通性，结果通过回调返回
    void testConnection(const std::function<void(bool, const std::string&)>& callback);

    /// 供进度条绘制：返回当前视频的可标记片段快照
    std::vector<SponsorMark> getMarks();

    /// 供设置页显示当前状态
    std::string status() const;

private:
    SponsorBlockHelper() = default;

    void fetchSegments();
    void recordViewed(const std::string& uuid);
    static bool categoryEnabled(const std::string& category);

    mutable std::mutex mutex;
    std::string currentBvid;
    uint64_t currentCid = 0;
    uint64_t requestId  = 0;
    std::vector<SponsorSegment> segments;

    // 不再永久记录已跳过的片段，改为短时间冷却：
    // 用户手动拖回广告前时，冷却过后仍能再次跳转。
    std::string lastSkippedUuid;
    std::chrono::steady_clock::time_point lastSkipAt{};
};
