#include "launcher/launcher.h"

#include <cstdint>
#include <esp_log.h>
#include <font_awesome.h>

#include "app_manager.h"
#include "board.h"
#include "display/lcd_display.h"
#include "display/lvgl_display/lvgl_theme.h"

#define TAG "Launcher"

// 设计规范(docs/ui/DESIGN.md)色板
static constexpr uint32_t kColorCard     = 0x252836; // 卡片/磁贴
static constexpr uint32_t kColorSelected = 0x2D3550; // 选中态背景
static constexpr uint32_t kColorAccent   = 0x4A9DFF; // 主强调
static constexpr uint32_t kColorTextMain = 0xE8EAF0; // 主文字
static constexpr uint32_t kColorTextSub  = 0x9AA0B4; // 次文字

// 布局:边距 12,内容宽 216,磁贴 216x44,间距 8
static constexpr int kMargin       = 12;
static constexpr int kContentWidth = 216;
static constexpr int kTileHeight   = 44;
static constexpr int kTileYPitch   = kTileHeight + 8; // 44 + 8

// 磁贴/返回键点击:user_data 为索引,-1=返回键,0=聊天,1..N=apps_[i-1]
// 切换会销毁启动器所在覆盖层,不能在事件回调内直接删,经 lv_async_call 延后
static void LauncherSwitchAsync(void* arg) {
    int index = (int)(intptr_t)arg;
    auto display = Board::GetInstance().GetDisplay();
    DisplayLockGuard lock(display);
    auto& manager = AppManager::GetInstance();
    if (index <= 0) {
        manager.BackToChat();
    } else {
        manager.SwitchTo(manager.apps()[index - 1]);
    }
}

static void LauncherItemClickCb(lv_event_t* e) {
    int index = (int)(intptr_t)lv_event_get_user_data(e);
    lv_async_call(LauncherSwitchAsync, (void*)(intptr_t)index);
}

void Launcher::Enter(lv_obj_t* parent) {
    auto display = Board::GetInstance().GetDisplay();
    auto lvgl_theme = static_cast<LvglTheme*>(display->GetTheme());
    auto text_font = lvgl_theme->text_font()->font();
    auto icon_font = lvgl_theme->icon_font()->font();

    items_.clear();
    selected_ = 0;

    /* 标题「应用」(次文字色) */
    title_label_ = lv_label_create(parent);
    lv_obj_set_pos(title_label_, 0, 8);
    lv_obj_set_width(title_label_, LV_HOR_RES);
    lv_obj_set_style_text_align(title_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(title_label_, text_font, 0);
    lv_obj_set_style_text_color(title_label_, lv_color_hex(kColorTextSub), 0);
    lv_label_set_text(title_label_, "应用");

    /* 返回聊天按钮:216x28,y=30,卡片色,触摸点击回聊天 */
    back_btn_ = lv_obj_create(parent);
    lv_obj_set_pos(back_btn_, kMargin, 30);
    lv_obj_set_size(back_btn_, kContentWidth, 28);
    lv_obj_set_style_radius(back_btn_, 8, 0);
    lv_obj_set_style_bg_color(back_btn_, lv_color_hex(kColorCard), 0);
    lv_obj_set_style_bg_opa(back_btn_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(back_btn_, 0, 0);
    lv_obj_set_style_pad_all(back_btn_, 0, 0);
    lv_obj_clear_flag(back_btn_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(back_btn_, LauncherItemClickCb, LV_EVENT_CLICKED, (void*)(intptr_t)-1);

    auto back_icon = lv_label_create(back_btn_);
    lv_obj_set_style_text_font(back_icon, icon_font, 0);
    lv_obj_set_style_text_color(back_icon, lv_color_hex(kColorTextSub), 0);
    lv_obj_align(back_icon, LV_ALIGN_LEFT_MID, 40, 0);
    lv_label_set_text(back_icon, FONT_AWESOME_ARROW_LEFT);

    auto back_label = lv_label_create(back_btn_);
    lv_obj_set_style_text_font(back_label, text_font, 0);
    lv_obj_set_style_text_color(back_label, lv_color_hex(kColorTextSub), 0);
    lv_obj_align(back_label, LV_ALIGN_LEFT_MID, 66, 0);
    lv_label_set_text(back_label, "返回聊天");

    /* 菜单磁贴:聊天 + N 个 App,y=66 起步,间距 8 */
    auto& manager = AppManager::GetInstance();
    int total = 1 + (int)manager.apps().size(); // 聊天 + N 个 App

    for (int i = 0; i < total; i++) {
        bool is_chat = (i == 0);
        const char* name = is_chat ? "聊天助手" : manager.apps()[i - 1]->name().c_str();
        // 图标:聊天=评论,天气=云晴,2048=手柄,音乐=音符
        const char* icon = FONT_AWESOME_COMMENT;
        if (!is_chat) {
            const std::string& app_name = manager.apps()[i - 1]->name();
            if (app_name == "weather-clock") {
                icon = FONT_AWESOME_CLOUD_SUN;
            } else if (app_name == "2048") {
                icon = FONT_AWESOME_GAMEPAD;
            } else if (app_name == "music") {
                icon = FONT_AWESOME_MUSIC;
            }
        }

        auto item = lv_obj_create(parent);
        lv_obj_set_pos(item, kMargin, 66 + i * kTileYPitch);
        lv_obj_set_size(item, kContentWidth, kTileHeight);
        lv_obj_set_style_radius(item, 10, 0);
        lv_obj_set_style_bg_color(item, lv_color_hex(kColorCard), 0);
        lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(item, 0, 0);
        lv_obj_set_style_pad_all(item, 0, 0);
        lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(item, LauncherItemClickCb, LV_EVENT_CLICKED, (void*)(intptr_t)i);

        /* 左 40px 图标区(主强调色) */
        auto icon_label = lv_label_create(item);
        lv_obj_set_style_text_font(icon_label, icon_font, 0);
        lv_obj_set_style_text_color(icon_label, lv_color_hex(kColorAccent), 0);
        lv_obj_align(icon_label, LV_ALIGN_LEFT_MID, 12, 0);
        lv_label_set_text(icon_label, icon);

        /* 名称文本(图标区右侧,主文字色) */
        auto label = lv_label_create(item);
        lv_obj_set_width(label, kContentWidth - 48 - 8);
        lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
        lv_obj_set_style_text_font(label, text_font, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(kColorTextMain), 0);
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 48, 0);
        lv_label_set_text(label, name);

        items_.push_back(item);
    }

    UpdateHighlight();
    ESP_LOGI(TAG, "Launcher UI ready, %d items", total);
}

void Launcher::Exit() {
    // 覆盖层由 AppManager 统一销毁,这里只清引用
    title_label_ = nullptr;
    back_btn_ = nullptr;
    items_.clear();
}

bool Launcher::OnButton(ButtonId id, bool long_press) {
    auto& manager = AppManager::GetInstance();
    int total = (int)items_.size();

    if (id == kButtonTouch && !long_press) {
        // 确认进入选中项
        if (selected_ == 0) {
            manager.BackToChat();
        } else {
            manager.SwitchTo(manager.apps()[selected_ - 1]);
        }
        return true;
    }
    if (id == kButtonTouch && long_press) {
        manager.BackToChat();
        return true;
    }
    if (id == kButtonVolumeUp && !long_press) {
        selected_ = (selected_ - 1 + total) % total;
        UpdateHighlight();
        return true;
    }
    if (id == kButtonVolumeDown && !long_press) {
        selected_ = (selected_ + 1) % total;
        UpdateHighlight();
        return true;
    }
    return false; // 长按 vol+/- 在启动器里无意义,交回系统(音量调节)
}

void Launcher::UpdateHighlight() {
    // 可能从未持锁的 OnButton 调入,自行加锁(锁为递归锁,Enter 里重入安全)
    auto display = Board::GetInstance().GetDisplay();
    DisplayLockGuard lock(display);
    for (int i = 0; i < (int)items_.size(); i++) {
        if (i == selected_) {
            lv_obj_set_style_bg_color(items_[i], lv_color_hex(kColorSelected), 0);
            lv_obj_set_style_border_width(items_[i], 2, 0);
            lv_obj_set_style_border_color(items_[i], lv_color_hex(kColorAccent), 0);
        } else {
            lv_obj_set_style_bg_color(items_[i], lv_color_hex(kColorCard), 0);
            lv_obj_set_style_border_width(items_[i], 0, 0);
        }
    }
}
