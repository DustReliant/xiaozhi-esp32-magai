#include "NavigationBar.h"
#include "assets/lang_config.h"  // Lang::Strings::BATTERY_NEED_CHARGE
#include <font_awesome_symbols.h>
#include <cstring>

NavigationBar::NavigationBar(const DisplayFonts &fonts) 
    : m_fonts(fonts)
{
    // 设置默认主题颜色
    current_theme_ = LIGHT_THEME;

    // lv_obj_set_style_text_font(container_, _fonts.text, 0);
    // lv_obj_set_style_text_font(network_label_, _fonts.icon_font, 0);
    // lv_obj_set_style_text_font(notification_label_, _fonts.text, 0);
}

NavigationBar::~NavigationBar()
{
    if (container_)            lv_obj_del(container_);
    if (low_battery_popup_)    lv_obj_del(low_battery_popup_);
}

void NavigationBar::create(lv_obj_t* parent, SettingsCallback on_settings) {
    // 保存主题默认为 light
    current_theme_ = LIGHT_THEME;

    // 1) 创建状态栏容器
    //--------------------------------------------------
    container_ = lv_obj_create(parent);
    // 宽度撑满屏幕，高度根据字体行高自动适配
    lv_obj_set_size(container_, LV_HOR_RES,LV_SIZE_CONTENT);//m_fonts.text_font->line_height
    // 水平弹性布局，子控件从左到右依次排列
    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_radius(container_, 0, 0);
    lv_obj_set_style_bg_color(container_, current_theme_.background, 0);
    lv_obj_set_style_text_color(container_, current_theme_.text, 0);
    // 内边距：左右各 10 像素，上下 2 像素
    // lv_obj_set_style_pad_hor(container_, 10, 0);
    // lv_obj_set_style_pad_ver(container_, 0, 0);
    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(container_, 0, 0);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_pad_column(container_, 0, 0);
    lv_obj_set_style_pad_left(container_, 10, 0);
    lv_obj_set_style_pad_right(container_, 10, 0);
    lv_obj_set_style_pad_top(container_, 2, 0);
    lv_obj_set_style_pad_bottom(container_, 2, 0);
    lv_obj_set_scrollbar_mode(container_, LV_SCROLLBAR_MODE_OFF);
    // 设置状态栏的内容垂直居中
    lv_obj_set_flex_align(container_, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    //--------------------------------------------------
    // 6) 网络状态图标
    //--------------------------------------------------
    network_label_ = lv_label_create(container_);
    lv_label_set_text(network_label_, FONT_AWESOME_WIFI);
    lv_obj_set_style_text_font(network_label_, m_fonts.icon_font, 0);
    // 左侧间隔
    lv_obj_set_style_margin_left(network_label_, 5, 0);
     //--------------------------------------------------
    // 2) 情感/AI 芯片图标（最左）
    //--------------------------------------------------
    // emotion_label_ = lv_label_create(container_);
    // lv_label_set_text(emotion_label_, FONT_AWESOME_AI_CHIP);
    // lv_obj_set_style_text_font(emotion_label_, m_fonts.icon_font, 0); 
    // // 图标与下一控件的间隔
    // lv_obj_set_style_margin_right(emotion_label_, 5, 0);

    //--------------------------------------------------
    // 3) 通知区域（居中、默认隐藏）
    //--------------------------------------------------
    // notification_label_ = lv_label_create(container_);
    // // 占据弹性空间，水平居中显示
    // lv_obj_set_flex_grow(notification_label_, 1);
    // lv_obj_set_style_text_align(notification_label_, LV_TEXT_ALIGN_CENTER, 0);
    // lv_label_set_text(notification_label_, "");
    // lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);

    //--------------------------------------------------
    // 4) 系统状态滚动文本（占一份弹性空间）
    //--------------------------------------------------
    // status_label_ = lv_label_create(container_);
    // lv_obj_set_flex_grow(status_label_, 1);
    // lv_label_set_long_mode(status_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    // lv_label_set_text(status_label_, Lang::Strings::INITIALIZING);

    //--------------------------------------------------
    // 5) 静音图标
    //--------------------------------------------------
    // mute_label_ = lv_label_create(container_);
    // lv_label_set_text(mute_label_, FONT_AWESOME_VOLUME_MUTE);
    // lv_obj_set_style_text_font(mute_label_, m_fonts.icon_font, 0);



    //--------------------------------------------------
    // 7) 电池状态图标
    //--------------------------------------------------
    // battery_label_ = lv_label_create(container_);
    // lv_label_set_text(battery_label_, FONT_AWESOME_BATTERY_FULL);
    // lv_obj_set_style_text_font(battery_label_, m_fonts.icon_font, 0);
    // lv_obj_set_style_margin_left(battery_label_, 5, 0);

    //--------------------------------------------------
    // 8) 设置按钮（可点击）
    //--------------------------------------------------
    settings_btn_ = lv_btn_create(container_);
    // 在按钮内创建图标
    lv_obj_t* icon = lv_label_create(settings_btn_);
    //lv_label_set_text(icon, FONT_AWESOME_COGS);
    lv_obj_center(icon);
    // TODO: 事件由 ScreenEventManager 绑定

    lv_obj_add_event_cb(settings_btn_, [](lv_event_t* e) {
        auto* nav_bar = static_cast<NavigationBar*>(lv_event_get_user_data(e));
        if (nav_bar && nav_bar->on_settings) {
            nav_bar->on_settings();
        }
    }, LV_EVENT_CLICKED, this);

    //--------------------------------------------------
    // 9) 低电量弹窗（隐藏）
    //--------------------------------------------------
    // low_battery_popup_ = lv_obj_create(parent);
    // lv_obj_set_size(low_battery_popup_, LV_HOR_RES * 0.9,
    //                 m_fonts.text_font->line_height * 2);
    // lv_obj_align(low_battery_popup_, LV_ALIGN_BOTTOM_MID, 0, 0);
    // lv_obj_set_style_bg_color(low_battery_popup_, current_theme_.low_battery, 0);
    // lv_obj_set_style_radius(low_battery_popup_, 10, 0);

    // // 弹窗内文字
    // low_battery_label_ = lv_label_create(low_battery_popup_);
    // lv_label_set_text(low_battery_label_, Lang::Strings::BATTERY_NEED_CHARGE);
    // lv_obj_set_style_text_color(low_battery_label_, lv_color_white(), 0);
    // lv_obj_center(low_battery_label_);
    // lv_obj_add_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);

    // 8) 低电量弹窗（隐藏）
    //createLowBatteryPopup(parent);
}

void NavigationBar::createLowBatteryPopup(lv_obj_t* parent) {
    low_battery_popup_ = lv_obj_create(parent);
    // lv_obj_set_size(low_battery_popup_, LV_HOR_RES * 0.9,
    //                 current_theme_.text.line_height * 2);
    lv_obj_align(low_battery_popup_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(low_battery_popup_, current_theme_.low_battery, 0);
    lv_obj_set_style_radius(low_battery_popup_, 10, 0);

    low_battery_label_ = lv_label_create(low_battery_popup_);
    lv_label_set_text(low_battery_label_, Lang::Strings::BATTERY_NEED_CHARGE);
    lv_obj_set_style_text_color(low_battery_label_, lv_color_white(), 0);
    lv_obj_center(low_battery_label_);
    lv_obj_add_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);
}

void NavigationBar::updateTheme(const ThemeColors& theme) {
    current_theme_ = theme;
    if (container_) {
        lv_obj_set_style_bg_color(container_, theme.background, 0);
        lv_obj_set_style_text_color(container_, theme.text, 0);
    }
    if (low_battery_popup_) {
        lv_obj_set_style_bg_color(low_battery_popup_, theme.low_battery, 0);
    }
}
