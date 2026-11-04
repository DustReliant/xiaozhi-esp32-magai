#include "heartbeat/heartbeat_app.h"

#include <esp_log.h>
#include <font_awesome.h>

#include "app_manager.h"
#include "board.h"
#include "display/lcd_display.h"
#include "display/lvgl_display/lvgl_theme.h"

#define TAG "Heartbeat"

// 设计规范(docs/ui/DESIGN.md)色板
static constexpr uint32_t kColorTextSub = 0x9AA0B4; // 次文字

/* Filled heart glyph enlarged to roughly 110px, with a soft color-matched glow. */
static constexpr int kHeartScaleIdle = 920;
static constexpr int kHeartScaleBeat = 1010;

/* 粒子:8 颗小圆点,从心向外飘散并渐隐 */
static constexpr int kParticles = 8;

void HeartbeatApp::BeatTimerCb(lv_timer_t* timer) {
    auto* app = static_cast<HeartbeatApp*>(lv_timer_get_user_data(timer));
    if (app->heart_label_ == nullptr) {
        return;
    }
    /* 心跳节律:两连跳后停(卢-塔节律),周期 4 步 × 150ms = 600ms ≈ 100 BPM */
    int scale = kHeartScaleIdle;
    bool beat = false;
    switch (app->beat_step_) {
        case 0: scale = kHeartScaleBeat; beat = true; break; // 第一跳(收缩)
        case 1: scale = kHeartScaleIdle; break;              // 舒张
        case 2: scale = (kHeartScaleBeat + kHeartScaleIdle) / 2; beat = true; break; // 第二跳
        default: scale = kHeartScaleIdle; break;             // 间歇
    }
    lv_obj_set_style_transform_scale(app->heart_label_, scale, 0);

    /* 第一跳时发射粒子,留出完整间歇让它们漂移、渐隐并隐藏。 */
    if (beat && app->beat_step_ == 0) {
        app->particle_phase_ = 0;
        for (int i = 0; i < kParticles && i < (int)(sizeof(app->particles_) / sizeof(app->particles_[0])); i++) {
            if (app->particles_[i] == nullptr) continue;
            int angle_idx = i - kParticles / 2;                 // -4..3
            int dx = angle_idx * 11 + (i % 2 ? 6 : -6);         // 左右扇形散开
            int dy = -26 - (i % 3) * 9;                         // 向上飘
            lv_obj_set_pos(app->particles_[i], 120 + dx, 96 + dy + 30);
            lv_obj_clear_flag(app->particles_[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_bg_opa(app->particles_[i], LV_OPA_COVER, 0);
        }
    } else if (app->particle_phase_ > 0) {
        for (int i = 0; i < kParticles && i < (int)(sizeof(app->particles_) / sizeof(app->particles_[0])); i++) {
            if (app->particles_[i] == nullptr) continue;
            lv_obj_move_to(app->particles_[i],
                           lv_obj_get_x(app->particles_[i]) + (i % 2 ? 3 : -3),
                           lv_obj_get_y(app->particles_[i]) - 5);
            lv_opa_t opa = (lv_opa_t)(LV_OPA_COVER - app->particle_phase_ * 70);
            lv_obj_set_style_bg_opa(app->particles_[i],
                                    opa > LV_OPA_TRANSP ? opa : static_cast<lv_opa_t>(LV_OPA_TRANSP), 0);
        }
        if (app->particle_phase_ >= 3) {
            for (int i = 0; i < kParticles; i++) {
                if (app->particles_[i] != nullptr) {
                    lv_obj_add_flag(app->particles_[i], LV_OBJ_FLAG_HIDDEN);
                }
            }
        }
    }
    if (beat || app->particle_phase_ > 0) {
        app->particle_phase_++;
    }
    app->beat_step_ = (app->beat_step_ + 1) % 4;
}

void HeartbeatApp::Enter(lv_obj_t* parent) {
    auto display = Board::GetInstance().GetDisplay();
    auto lvgl_theme = static_cast<LvglTheme*>(display->GetTheme());
    auto large_icon_font = lvgl_theme->large_icon_font()->font();

    parent_ = parent;
    mode_ = 0;

    title_label_ = lv_label_create(parent);
    lv_obj_set_width(title_label_, LV_HOR_RES);
    lv_obj_set_pos(title_label_, 0, 24);
    lv_obj_set_style_text_align(title_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(title_label_, lvgl_theme->text_font()->font(), 0);
    lv_obj_set_style_text_color(title_label_, lv_color_hex(kColorTextSub), 0);
    lv_label_set_text(title_label_, "心跳节奏");

    heart_label_ = lv_label_create(parent);
    lv_obj_set_style_text_font(heart_label_, large_icon_font, 0);
    lv_label_set_text(heart_label_, FONT_AWESOME_HEART);
    lv_obj_set_style_text_color(heart_label_, lv_color_hex(kHeartColors[mode_]), 0);
    lv_obj_set_style_transform_pivot_x(heart_label_, lv_obj_get_width(heart_label_) / 2, 0);
    lv_obj_set_style_transform_pivot_y(heart_label_, lv_obj_get_height(heart_label_) / 2, 0);
    lv_obj_set_style_shadow_color(heart_label_, lv_color_hex(kHeartColors[mode_]), 0);
    lv_obj_set_style_shadow_width(heart_label_, 16, 0);
    lv_obj_set_style_shadow_opa(heart_label_, LV_OPA_40, 0);
    lv_obj_align(heart_label_, LV_ALIGN_CENTER, 0, -8);
    lv_obj_add_flag(heart_label_, LV_OBJ_FLAG_GESTURE_BUBBLE);

    mode_label_ = lv_label_create(parent);
    lv_obj_set_width(mode_label_, LV_HOR_RES);
    lv_obj_set_pos(mode_label_, 0, 218);
    lv_obj_set_style_text_align(mode_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(mode_label_, lvgl_theme->text_font()->font(), 0);
    lv_obj_set_style_text_color(mode_label_, lv_color_hex(kHeartColors[mode_]), 0);
    lv_label_set_text(mode_label_, kModeNames[mode_]);

    for (int i = 0; i < kModeCount; ++i) {
        mode_dots_[i] = lv_obj_create(parent);
        lv_obj_set_size(mode_dots_[i], i == mode_ ? 10 : 7, i == mode_ ? 10 : 7);
        lv_obj_set_style_radius(mode_dots_[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(mode_dots_[i], lv_color_hex(kHeartColors[i]), 0);
        lv_obj_set_style_bg_opa(mode_dots_[i], i == mode_ ? LV_OPA_COVER : LV_OPA_40, 0);
        lv_obj_set_style_border_width(mode_dots_[i], 0, 0);
        lv_obj_set_style_pad_all(mode_dots_[i], 0, 0);
        lv_obj_align(mode_dots_[i], LV_ALIGN_TOP_MID, (i - 1) * 20, 250);
        lv_obj_clear_flag(mode_dots_[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(mode_dots_[i], LV_OBJ_FLAG_CLICKABLE);
    }
    lv_obj_add_event_cb(parent, GestureCb, LV_EVENT_GESTURE, this);

    /* 粒子层:8 颗 4-6px 红点,初始隐藏 */
    for (int i = 0; i < kParticles; i++) {
        particles_[i] = lv_obj_create(parent);
        int size = 4 + (i % 3) * 2; // 4/6/8px 混合
        lv_obj_set_size(particles_[i], size, size);
        lv_obj_set_style_radius(particles_[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(particles_[i], lv_color_hex(kHeartColors[mode_]), 0);
        lv_obj_set_style_bg_opa(particles_[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(particles_[i], 0, 0);
        lv_obj_set_style_pad_all(particles_[i], 0, 0);
        lv_obj_clear_flag(particles_[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(particles_[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(particles_[i], LV_OBJ_FLAG_HIDDEN);
    }

    beat_step_ = 0;
    particle_phase_ = 0;
    beat_timer_ = lv_timer_create(BeatTimerCb, 150, this);
    ESP_LOGI(TAG, "heartbeat started");
}

void HeartbeatApp::Exit() {
    if (beat_timer_ != nullptr) {
        lv_timer_del(beat_timer_);
        beat_timer_ = nullptr;
    }

    if (parent_ != nullptr) {
        lv_obj_remove_event_cb_with_user_data(parent_, GestureCb, this);
        parent_ = nullptr;
    }

    if (title_label_ != nullptr) {
        lv_obj_del(title_label_);
        title_label_ = nullptr;
    }
    if (heart_label_ != nullptr) {
        lv_obj_del(heart_label_);
        heart_label_ = nullptr;
    }
    if (mode_label_ != nullptr) {
        lv_obj_del(mode_label_);
        mode_label_ = nullptr;
    }
    for (auto& dot : mode_dots_) {
        if (dot != nullptr) {
            lv_obj_del(dot);
            dot = nullptr;
        }
    }
    for (auto& p : particles_) {
        if (p != nullptr) {
            lv_obj_del(p);
            p = nullptr;
        }
    }
    particle_phase_ = 0;
}

bool HeartbeatApp::OnButton(ButtonId id, bool long_press) {
    if (id == kButtonTouch && long_press) {
        AppManager::GetInstance().BackToChat();
        return true;
    }
    if (!long_press && id == kButtonVolumeUp) {
        auto display = Board::GetInstance().GetDisplay();
        DisplayLockGuard lock(display);
        SetMode((mode_ + 1) % kModeCount);
        return true;
    }
    if (!long_press && id == kButtonVolumeDown) {
        auto display = Board::GetInstance().GetDisplay();
        DisplayLockGuard lock(display);
        SetMode((mode_ + kModeCount - 1) % kModeCount);
        return true;
    }
    return false;
}

void HeartbeatApp::GestureCb(lv_event_t* event) {
    auto* app = static_cast<HeartbeatApp*>(lv_event_get_user_data(event));
    auto* indev = lv_event_get_indev(event);
    if (app == nullptr || indev == nullptr || app->heart_label_ == nullptr) {
        return;
    }
    lv_dir_t direction = lv_indev_get_gesture_dir(indev);
    if (direction == LV_DIR_LEFT) {
        app->SetMode((app->mode_ + 1) % kModeCount);
    } else if (direction == LV_DIR_RIGHT) {
        app->SetMode((app->mode_ + kModeCount - 1) % kModeCount);
    }
}

void HeartbeatApp::SetMode(int mode) {
    mode_ = (mode % kModeCount + kModeCount) % kModeCount;
    uint32_t color = kHeartColors[mode_];
    if (heart_label_ != nullptr) {
        lv_obj_set_style_text_color(heart_label_, lv_color_hex(color), 0);
        lv_obj_set_style_shadow_color(heart_label_, lv_color_hex(color), 0);
    }
    if (mode_label_ != nullptr) {
        lv_obj_set_style_text_color(mode_label_, lv_color_hex(color), 0);
        lv_label_set_text(mode_label_, kModeNames[mode_]);
    }
    for (int i = 0; i < kModeCount; ++i) {
        if (mode_dots_[i] == nullptr) continue;
        lv_obj_set_size(mode_dots_[i], i == mode_ ? 10 : 7, i == mode_ ? 10 : 7);
        lv_obj_set_style_bg_opa(mode_dots_[i], i == mode_ ? LV_OPA_COVER : LV_OPA_40, 0);
    }
    for (auto* particle : particles_) {
        if (particle != nullptr) {
            lv_obj_set_style_bg_color(particle, lv_color_hex(color), 0);
        }
    }
}
