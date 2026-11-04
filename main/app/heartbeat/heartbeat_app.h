#ifndef MAGAI_HEARTBEAT_APP_H
#define MAGAI_HEARTBEAT_APP_H

#include "app/app.h"

/**
 * @brief 心跳 App
 *
 * 屏幕中央一颗跳动的红心(LVGL 缩放动画模拟心跳节律:双跳-停)。
 * 纯本地动画,无网络;触摸长按退出。
 */
class HeartbeatApp : public App {
public:
    static HeartbeatApp& GetInstance() {
        static HeartbeatApp instance;
        return instance;
    }

    void Enter(lv_obj_t* parent) override;
    void Exit() override;
    bool OnButton(ButtonId id, bool long_press) override;

private:
    HeartbeatApp() : App("heartbeat") {}

    static constexpr int kModeCount = 3;
    static constexpr uint32_t kHeartColors[kModeCount] = {0xFF4F73, 0xFFB547, 0x55C8B2};
    static constexpr const char* kModeNames[kModeCount] = {"玫瑰", "暖阳", "薄荷"};

    void SetMode(int mode);
    static void GestureCb(lv_event_t* event);

    lv_obj_t* parent_ = nullptr;
    lv_obj_t* title_label_ = nullptr;
    lv_obj_t* heart_label_ = nullptr;
    lv_obj_t* mode_label_ = nullptr;
    lv_obj_t* mode_dots_[kModeCount] = {nullptr, nullptr, nullptr};
    lv_obj_t* particles_[8] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
    lv_timer_t* beat_timer_ = nullptr;
    int beat_step_ = 0;
    int particle_phase_ = 0; // 0=待触发,>0=漂移中
    int mode_ = 0;

    static void BeatTimerCb(lv_timer_t* timer);
};

#endif // MAGAI_HEARTBEAT_APP_H
