#ifndef DISPLAY_H
#define DISPLAY_H

#include <lvgl.h>
#include <esp_timer.h>
#include <esp_log.h>
#include <esp_pm.h>

#include <string>
#include "theme_fonts.h"

class Display {
public:
    Display();
    virtual ~Display();

    virtual void SetStatus(const char* status);
    virtual void ShowNotification(const char* notification, int duration_ms = 3000);
    virtual void ShowNotification(const std::string &notification, int duration_ms = 3000);
    virtual void SetEmotion(const char* emotion);
    virtual void SetChatMessage(const char* role, const char* content);
    virtual void SetIcon(const char* icon);
    virtual void SetTheme(const std::string& theme_name);
    virtual void SetUIStyle(UIStyle style);
    virtual std::string GetTheme() { return current_theme_name_; }

    inline int width() const { return width_; }
    inline int height() const { return height_; }

protected:
    int width_ = 0;
    int height_ = 0;
    
    esp_pm_lock_handle_t pm_lock_ = nullptr;
    lv_display_t *display_ = nullptr;

    // 用于显示情感状态的标签控件（LVGL对象指针，通常作为类成员变量）
    lv_obj_t *emotion_label_ = nullptr;
    // 用于显示网络连接状态的标签控件（如WiFi/移动网络状态）
    lv_obj_t *network_label_ = nullptr;
    // 显示系统总体状态信息的标签控件（如待机/工作中状态）
    lv_obj_t *status_label_ = nullptr;
    // 用于显示通知消息的标签控件（如新消息提醒）
    lv_obj_t *notification_label_ = nullptr;
    // 静音状态指示标签控件（显示静音图标/文字）
    lv_obj_t *mute_label_ = nullptr;
    // 电池电量显示标签控件（显示电量百分比/图标）
    lv_obj_t *battery_label_ = nullptr;
    // 聊天消息内容显示标签控件（用于展示对话消息）
    lv_obj_t* chat_message_label_ = nullptr;
    // 低电量警告弹窗控件（当电量过低时弹出的模态对话框）
    lv_obj_t* low_battery_popup_ = nullptr;
    // 低电量警告标签控件（LVGL对象指针，通常显示在状态栏或弹窗中）
    lv_obj_t* low_battery_label_ = nullptr;
    
    const char* battery_icon_ = nullptr;
    const char* network_icon_ = nullptr;
    bool muted_ = false;
    std::string current_theme_name_;

    esp_timer_handle_t notification_timer_ = nullptr;
    esp_timer_handle_t update_timer_ = nullptr;

    friend class DisplayLockGuard;
    virtual bool Lock(int timeout_ms = 0) = 0;
    virtual void Unlock() = 0;

    virtual void Update();
};


class DisplayLockGuard {
public:
    DisplayLockGuard(Display *display) : display_(display) {
        if (!display_->Lock(30000)) {
            ESP_LOGE("Display", "Failed to lock display");
        }
    }
    ~DisplayLockGuard() {
        display_->Unlock();
    }

private:
    Display *display_;
};

class NoDisplay : public Display {
private:
    virtual bool Lock(int timeout_ms = 0) override {
        return true;
    }
    virtual void Unlock() override {}
};

#endif
