#pragma once

#include <lvgl.h>
#include <string>
#include <vector>
#include <functional>
#include "ui_interface.h"
#include "../theme_fonts.h"

class NavigationBar {
public:
    // 点击“设置”图标时的回调类型
    using SettingsCallback = std::function<void()>;

    NavigationBar() = default;
    NavigationBar(const DisplayFonts& fonts);
    ~NavigationBar();

    static lv_obj_t* createSettingsButton(lv_obj_t* parent);

    // 设置按钮点击回调，你也可以改成其他回调签名
    std::function<void()> on_settings;


    lv_obj_t* GetNavSettingsButton() const { return settings_btn_; }

    // 在父容器 parent 上创建导航栏
    void create(lv_obj_t* parent, SettingsCallback on_settings);

    // 动态更新主题颜色
    void updateTheme(const ThemeColors& theme);

    // 获取容器对象，以添加到布局中
    lv_obj_t* getContainer() const { return container_; }

    

private:
    // 内部创建各个控件
    void createStatusBar(lv_obj_t* parent);
    void createLowBatteryPopup(lv_obj_t* parent);

    // 状态栏主容器（横向一排：网络|通知|状态|静音|电池|设置）
    lv_obj_t* container_ = nullptr;

    // 状态栏各子控件
    lv_obj_t* emotion_label_      = nullptr;  // AI芯片图标/表情
    lv_obj_t* notification_label_ = nullptr;  // 通知提示文本
    lv_obj_t* status_label_       = nullptr;  // 系统状态滚动文本
    lv_obj_t* mute_label_         = nullptr;  // 静音图标
    lv_obj_t* network_label_      = nullptr;  // 网络状态图标
    lv_obj_t* battery_label_      = nullptr;  // 电池状态图标
    lv_obj_t* settings_btn_       = nullptr;  // 设置入口按钮

    // 低电量弹窗
    lv_obj_t* low_battery_popup_ = nullptr;  
    lv_obj_t* low_battery_label_ = nullptr;  

    // 当前主题，用于 updateTheme
    ThemeColors current_theme_;
    // 当前字体，用于设置字体样式
    DisplayFonts m_fonts;
};
