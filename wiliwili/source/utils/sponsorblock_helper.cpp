//
// SponsorBlock (空降助手) integration for wiliwili
//

#include "utils/sponsorblock_helper.hpp"

#include <algorithm>
#include <cmath>
#include <nlohmann/json.hpp>
#include <borealis/core/logger.hpp>

#include "api/bilibili/util/http.hpp"
#include "view/mpv_core.hpp"

namespace {
/// 同一片段的最小跳转间隔：防止 seek 落点不准造成来回抽动，
/// 同时允许用户手动拖回广告前之后再次跳转。
constexpr int SKIP_COOLDOWN_SECONDS = 4;
/// 命中判定提前量，避免播放进度回调一步跨过片段起点。
constexpr double LOOKAHEAD_SECONDS = 0.3;
}  // namespace

bool SponsorBlockHelper::categoryEnabled(const std::string& category) {
    if (category == "sponsor") return true;
    if (category == "selfpromo") return MODE >= 1;
    return MODE >= 2;
}

void SponsorBlockHelper::setVideo(const std::string& bvid, uint64_t cid) {
    {
        std::lock_guard<std::mutex> lock(mutex);
        currentBvid = bvid;
        currentCid  = cid;
        segments.clear();
        lastSkippedUuid.clear();
        lastSkipAt = std::chrono::steady_clock::time_point{};
        requestId++;
    }

    if (!ENABLED || bvid.empty() || cid == 0) {
        return;
    }
    fetchSegments();
}

void SponsorBlockHelper::clear() {
    std::lock_guard<std::mutex> lock(mutex);
    currentBvid.clear();
    currentCid = 0;
    requestId++;
    segments.clear();
    lastSkippedUuid.clear();
    lastSkipAt = std::chrono::steady_clock::time_point{};
}

void SponsorBlockHelper::fetchSegments() {
    std::string bvid;
    uint64_t cid   = 0;
    uint64_t reqId = 0;
    {
        std::lock_guard<std::mutex> lock(mutex);
        bvid  = currentBvid;
        cid   = currentCid;
        reqId = requestId;
    }
    if (bvid.empty() || cid == 0) {
        return;
    }

    brls::Logger::debug("SponsorBlock: request segments for {} / {}", bvid, cid);

    bilibili::HTTP::_cpr_get(
        SERVER + "/api/skipSegments",
        cpr::Parameters{{"videoID", bvid}, {"cid", std::to_string(cid)}},
        [this, bvid, cid, reqId](const cpr::Response& r) {
            if (r.status_code != 200) {
                brls::Logger::debug("SponsorBlock: HTTP status {}", r.status_code);
                return;
            }

            try {
                auto json = nlohmann::json::parse(r.text);
                if (!json.is_array()) {
                    return;
                }

                std::vector<SponsorSegment> parsed;
                for (const auto& item : json) {
                    if (!item.contains("segment") || !item["segment"].is_array() || item["segment"].size() != 2) {
                        continue;
                    }

                    auto category = item.value("category", "");
                    if (!categoryEnabled(category)) {
                        continue;
                    }
                    // 只处理明确标记为跳过的片段，mute / poi / full 不参与
                    if (item.value("actionType", "skip") != "skip") {
                        continue;
                    }

                    double start = item["segment"][0].get<double>();
                    double end   = item["segment"][1].get<double>();
                    if (!(end > start) || start < 0) {
                        continue;
                    }
                    if (end - start < MIN_DURATION) {
                        continue;
                    }

                    parsed.push_back({start, end, item.value("UUID", ""), category});
                }

                std::sort(parsed.begin(), parsed.end(),
                          [](const SponsorSegment& a, const SponsorSegment& b) { return a.start < b.start; });

                std::lock_guard<std::mutex> lock(mutex);
                // 丢弃过期响应，避免切换视频后旧片段被写回
                if (reqId != requestId || bvid != currentBvid || cid != currentCid) {
                    return;
                }
                segments = std::move(parsed);
                brls::Logger::info("SponsorBlock: loaded {} segments for {}", segments.size(), bvid);
            } catch (const std::exception& e) {
                brls::Logger::error("SponsorBlock: parse error: {}", e.what());
            }
        },
        [](const std::string& message, int code) {
            brls::Logger::debug("SponsorBlock: request failed ({}) {}", code, message);
        });
}

void SponsorBlockHelper::checkAndSkip() {
    if (!ENABLED || MPVCore::instance().video_stopped) {
        return;
    }

    double time = MPVCore::instance().playback_time;
    if (time <= 0) {
        return;
    }

    std::vector<SponsorSegment> snapshot;
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (segments.empty()) {
            return;
        }
        snapshot = segments;
    }

    for (const auto& segment : snapshot) {
        // 半开区间 + 提前量：位置在 [start - 0.3, end) 内命中
        if (time + LOOKAHEAD_SECONDS < segment.start || time >= segment.end) {
            continue;
        }

        bool shouldSkip = true;
        {
            std::lock_guard<std::mutex> lock(mutex);
            auto now = std::chrono::steady_clock::now();
            if (!lastSkippedUuid.empty() && lastSkippedUuid == segment.uuid &&
                std::chrono::duration_cast<std::chrono::seconds>(now - lastSkipAt).count() < SKIP_COOLDOWN_SECONDS) {
                shouldSkip = false;
            }
            if (shouldSkip) {
                lastSkippedUuid = segment.uuid;
                lastSkipAt      = now;
            }
        }
        if (!shouldSkip) {
            continue;
        }

        int64_t target = static_cast<int64_t>(std::ceil(segment.end + 0.1));
        brls::Logger::info("SponsorBlock: skip [{}, {}] -> {}", segment.start, segment.end, target);
        MPVCore::instance().seek(target);
        if (SHOW_TOAST) {
            MPVCore::instance().showOsdText("空降助手：已跳过广告片段", 2000);
        }
        recordViewed(segment.uuid);
        break;
    }
}

void SponsorBlockHelper::testConnection(const std::function<void(bool, const std::string&)>& callback) {
    bilibili::HTTP::_cpr_get(
        SERVER + "/api/ready",
        cpr::Parameters{},
        [callback](const cpr::Response& r) {
            if (r.status_code == 200) {
                callback(true, "连接正常 (HTTP 200)");
            } else {
                callback(false, "HTTP " + std::to_string(r.status_code));
            }
        },
        [callback](const std::string& message, int code) {
            std::string reason = message.empty() ? "请求失败" : message;
            callback(false, reason + " (" + std::to_string(code) + ")");
        });
}

std::vector<SponsorMark> SponsorBlockHelper::getMarks() {
    if (!ENABLED || !PROGRESS_MARK) {
        return {};
    }
    std::lock_guard<std::mutex> lock(mutex);
    std::vector<SponsorMark> marks;
    marks.reserve(segments.size());
    for (const auto& segment : segments) {
        marks.push_back({segment.start, segment.end, segment.category});
    }
    return marks;
}

std::string SponsorBlockHelper::status() const {
    std::lock_guard<std::mutex> lock(mutex);
    return currentBvid.empty() ? "未加载视频" : currentBvid + " / " + std::to_string(segments.size()) + " 个片段";
}

void SponsorBlockHelper::recordViewed(const std::string& uuid) {
    if (uuid.empty()) {
        return;
    }
    bilibili::HTTP::_cpr_post(
        SERVER + "/api/viewedVideoSponsorTime",
        cpr::Parameters{{"UUID", uuid}},
        cpr::Payload{},
        [](const cpr::Response&) {},
        [](const std::string&, int) {});
}
