//
// Created by fang on 2022/8/15.
//

#include <borealis/views/rectangle.hpp>
#include <borealis/core/application.hpp>
#include <borealis/core/touch/pan_gesture.hpp>
#include <borealis/core/touch/tap_gesture.hpp>

#include "view/video_progress_slider.hpp"
#include "view/svg_image.hpp"
#include "utils/sponsorblock_helper.hpp"
#include "view/mpv_core.hpp"

namespace {
NVGcolor sponsorMarkColor(const std::string& category) {
    if (category == "sponsor") return nvgRGBf(1.0f, 0.72f, 0.0f);
    if (category == "selfpromo") return nvgRGBf(0.0f, 0.78f, 1.0f);
    if (category == "intro") return nvgRGBf(0.65f, 0.45f, 1.0f);
    if (category == "outro") return nvgRGBf(0.35f, 0.55f, 1.0f);
    if (category == "interaction") return nvgRGBf(0.35f, 0.85f, 0.45f);
    if (category == "filler" || category == "preview") return nvgRGBf(0.9f, 0.45f, 0.65f);
    return nvgRGBf(0.75f, 0.75f, 0.75f);
}
}  // namespace


VideoProgressSlider::VideoProgressSlider() {
    input = brls::Application::getPlatform()->getInputManager();

    line        = new brls::Rectangle();
    lineEmpty   = new brls::Rectangle();
    pointerIcon = new SVGImage();
    pointer     = new brls::Box();

    line->detach();
    lineEmpty->detach();
    pointer->detach();

    setHeight(40);

    line->setHeight(7);
    line->setCornerRadius(3.5f);

    lineEmpty->setHeight(7);
    lineEmpty->setCornerRadius(3.5f);

    pointerIcon->setDimensions(44, 44);
    pointerIcon->setImageFromSVGRes("svg/bpx-svg-sprite-thumb.svg");

    pointer->setDimensions(60, 60);
    pointer->setFocusable(true);
    pointer->setHighlightCornerRadius(60);
    pointer->setHideHighlightBackground(true);
    pointer->setHideClickAnimation(true);
    pointer->setAlignItems(brls::AlignItems::CENTER);
    pointer->setJustifyContent(brls::JustifyContent::CENTER);
    pointer->addView(pointerIcon);
    this->forwardXMLAttribute("focusUp", pointer);
    this->forwardXMLAttribute("focusRight", pointer);

    pointer->registerClickAction([this](...) {
        this->setManuallyMode();
        return true;
    });

    pointer->registerAction("left", brls::BUTTON_NAV_LEFT, [this](...) { return pointerSelected; }, true, true);

    pointer->registerAction("right", brls::BUTTON_NAV_RIGHT, [this](...) { return pointerSelected; }, true, true);

    pointer->registerAction("up", brls::BUTTON_NAV_UP, [this](...) {
        if (pointerSelected) pointer->shakeHighlight(brls::FocusDirection::UP);
        return pointerSelected;
    }, true, true);

    pointer->registerAction("down", brls::BUTTON_NAV_DOWN, [this](...) {
        if (pointerSelected) pointer->shakeHighlight(brls::FocusDirection::DOWN);
        return pointerSelected;
    }, true, true);

    pointer->registerAction("cancel", brls::BUTTON_B, [this](...) {
        return cancelPointerChange();
    });

    addView(pointer);
    addView(line);
    addView(lineEmpty);

    brls::Theme theme = brls::Application::getTheme();

    line->setColor(theme["brls/slider/line_filled"]);
    lineEmpty->setColor(theme["brls/slider/line_empty"]);

    pointer->addGestureRecognizer(new brls::PanGestureRecognizer(
        [this](brls::PanGestureStatus status, brls::Sound* soundToPlay) {
            brls::Application::giveFocus(pointer);

            static float lastProgress = progress;

            if (status.state == brls::GestureState::UNSURE) {
                *soundToPlay = brls::SOUND_FOCUS_CHANGE;
                return;
            }

            else if (status.state == brls::GestureState::INTERRUPTED || status.state == brls::GestureState::FAILED) {
                *soundToPlay = brls::SOUND_TOUCH_UNFOCUS;
                return;
            }

            else if (status.state == brls::GestureState::START) {
                lastProgress = progress;
            }

            float paddingWidth = getWidth() - pointer->getWidth();
            float delta        = status.position.x - status.startPosition.x;

            setProgress(lastProgress + delta / paddingWidth);
            progressEvent.fire(this->progress);

            if (status.state == brls::GestureState::END) {
                brls::Application::getPlatform()->getAudioPlayer()->play(brls::SOUND_SLIDER_RELEASE);
                progressSetEvent.fire(this->progress);
                brls::Application::giveFocus(this->getParentActivity()->getContentView());
            }
        },
        brls::PanAxis::HORIZONTAL));

    this->addGestureRecognizer(
        new brls::TapGestureRecognizer([this](brls::TapGestureStatus status, brls::Sound* soundToPlay) {
            if (status.state != brls::GestureState::END) return;
            float paddingWidth = getWidth() - pointer->getWidth();
            float delta        = status.position.x - pointer->getWidth() / 2 - pointer->getX();
            setProgress(progress + delta / paddingWidth);
            progressSetEvent.fire(this->progress);
            brls::Application::giveFocus(this->getParentActivity()->getContentView());
        }));

    progress     = 0;
    lastProgress = 0;
}

brls::View* VideoProgressSlider::create() { return new VideoProgressSlider(); }

void VideoProgressSlider::onLayout() {
    Box::onLayout();
    updateUI();
}

brls::View* VideoProgressSlider::getDefaultFocus() { return pointer; }

void VideoProgressSlider::setProgress(float progress) {
    lastProgress = progress;
    if (ignoreProgressSetting) return;

    this->progress = progress;
    if (this->progress < 0) this->progress = 0;
    if (this->progress > 1) this->progress = 1;
    updateUI();
}

void VideoProgressSlider::updateUI() {
    float paddingWidth   = getWidth() - pointer->getWidth();
    float lineStart      = pointer->getWidth() / 2;
    float lineStartWidth = paddingWidth * progress;
    float lineEnd        = paddingWidth * progress + pointer->getWidth() / 2;
    float lineEndWidth   = paddingWidth * (1 - progress);
    float lineYPos       = getHeight() / 2 - line->getHeight() / 2;

    line->setDetachedPosition(lineStart, lineYPos);
    line->setWidth(lineStartWidth);

    lineEmpty->setDetachedPosition(round(lineEnd), lineYPos);
    lineEmpty->setWidth(lineEndWidth);

    pointer->setDetachedPosition(lineEnd - pointer->getWidth() / 2, getHeight() / 2 - pointer->getHeight() / 2);
}

VideoProgressSlider::~VideoProgressSlider() = default;

void VideoProgressSlider::addClipPoint(float point) { clipPointList.emplace_back(point); }

void VideoProgressSlider::clearClipPoint() { clipPointList.clear(); }

void VideoProgressSlider::setClipPoint(const std::vector<float>& data) { clipPointList = data; }

const std::vector<float>& VideoProgressSlider::getClipPoint() { return clipPointList; }

void VideoProgressSlider::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
                               brls::FrameContext* ctx) {
    if (pointerSelected) {
        buttonsProcessing();
    }

    for (View* child : this->getChildren()) {
        if (child == this->pointer) {
            // draw clip point before pointer
            nvgBeginPath(vg);
            nvgFillColor(vg, a(nvgRGBf(1.0f, 1.0f, 1.0f)));
            for (auto& i : clipPointList) {
                nvgCircle(vg, x + 32 + i * (width - 64), y + height / 2, 3);
            }
            nvgFill(vg);
        }
        child->frame(ctx);
    }

    // 空降助手：在进度条上叠加按分类着色的片段区间
    auto marks = SponsorBlockHelper::instance().getMarks();
    auto duration = MPVCore::instance().duration;
    if (!marks.empty() && duration > 0) {
        float trackX     = x + 30;
        float trackWidth = width - 60;
        if (trackWidth > 0) {
            float markHeight = 4;
            float markY      = y + height / 2 - markHeight / 2;
            for (const auto& mark : marks) {
                float start = static_cast<float>(mark.start / duration);
                float end   = static_cast<float>(mark.end / duration);
                if (start < 0) start = 0;
                if (end > 1) end = 1;
                if (start > 1 || end < 0 || end <= start) continue;

                float x1 = trackX + start * trackWidth;
                float x2 = trackX + end * trackWidth;
                if (x2 - x1 < 1) x2 = x1 + 1;

                nvgBeginPath(vg);
                nvgRoundedRect(vg, x1, markY, x2 - x1, markHeight, 2);
                nvgFillColor(vg, a(sponsorMarkColor(mark.category)));
                nvgFill(vg);
            }
        }
    }
}

void VideoProgressSlider::buttonsProcessing() {
    auto& state        = brls::Application::getControllerState();
    static bool repeat = false;
    static brls::ControllerState lastState = state;

    if (state.buttons[brls::BUTTON_NAV_RIGHT] && state.buttons[brls::BUTTON_NAV_LEFT]) return;

    // 在移动光标的中途放开按键，重新计算起始进度
    if (lastState.buttons[brls::BUTTON_NAV_RIGHT] != state.buttons[brls::BUTTON_NAV_RIGHT] ||
        lastState.buttons[brls::BUTTON_NAV_LEFT] != state.buttons[brls::BUTTON_NAV_LEFT] ) {
        lastStartProgress = progress;
    }

    float step = 0.2f;
    if (progressUpdater) {
        step = progressUpdater(progress - lastStartProgress);
    }

    if (state.buttons[brls::BUTTON_NAV_RIGHT]) {
        progress += step / brls::Application::getFPS();
        if (progress >= 1 && !repeat) {
            repeat = true;
            pointer->shakeHighlight(brls::FocusDirection::RIGHT);
        }
    }

    if (state.buttons[brls::BUTTON_NAV_LEFT]) {
        progress -= step / brls::Application::getFPS();
        if (progress <= 0 && !repeat) {
            repeat = true;
            pointer->shakeHighlight(brls::FocusDirection::LEFT);
        }
    }

    if (progress > 1) progress = 1;
    if (progress < 0) progress = 0;
    progressEvent.fire(progress);
    updateUI();

    if ((!state.buttons[brls::BUTTON_NAV_RIGHT] && !state.buttons[brls::BUTTON_NAV_LEFT]) ||
        (progress > 0.01f && progress < 0.99f)) {
        repeat = false;
    }
    lastState = state;
}

void VideoProgressSlider::onChildFocusLost(brls::View* directChild, brls::View* focusedView) {
    Box::onChildFocusLost(directChild, focusedView);
    if (directChild == pointer) {
        cancelPointerChange();
    }
}

bool VideoProgressSlider::cancelPointerChange() {
    if (!pointerSelected) return false;
    pointerSelected       = false;
    ignoreProgressSetting = false;
    pointer->setHideHighlightBackground(true);
    this->progress = lastProgress;
    if (this->progress < 0) this->progress = 0;
    if (this->progress > 1) this->progress = 1;
    updateUI();
    progressCancelEvent.fire();
    return true;
}

void VideoProgressSlider::setManuallyMode() {
    lastStartProgress = progress;
    pointerSelected = !pointerSelected;
    pointer->setHideHighlightBackground(!pointerSelected);
    ignoreProgressSetting = pointerSelected;
    if (!pointerSelected) progressSetEvent.fire(this->progress);
}