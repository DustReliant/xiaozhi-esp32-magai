#ifndef THEME_FONTS_H
#define THEME_FONTS_H

#include <lvgl.h>
#include <memory>
#include <string>

// 深色主题颜色定义
#define DARK_BACKGROUND_COLOR       lv_color_hex(0x030303)     // 深色背景
#define DARK_TEXT_COLOR             lv_color_white()           // 白色文本
#define DARK_CHAT_BACKGROUND_COLOR  lv_color_hex(0x1E1E1E)     // 比背景色稍亮
#define DARK_USER_BUBBLE_COLOR      lv_color_hex(0x1A6C37)     // 深绿色
#define DARK_ASSISTANT_BUBBLE_COLOR lv_color_hex(0x030303)     // 深灰色
#define DARK_SYSTEM_BUBBLE_COLOR    lv_color_hex(0x2A2A2A)     // 中灰色
#define DARK_SYSTEM_TEXT_COLOR      lv_color_hex(0xAAAAAA)     // 浅灰色文本
#define DARK_BORDER_COLOR           lv_color_hex(0x333333)     // 深灰色边框
#define DARK_LOW_BATTERY_COLOR      lv_color_hex(0xFF0000)     // 深色模式下的红色

// 浅色主题颜色定义
#define LIGHT_BACKGROUND_COLOR       lv_color_white()           // 白色背景
#define LIGHT_TEXT_COLOR             lv_color_black()           // 黑色文本
#define LIGHT_CHAT_BACKGROUND_COLOR  lv_color_hex(0xE0E0E0)     // 浅灰色背景
#define LIGHT_USER_BUBBLE_COLOR      lv_color_hex(0x95EC69)     // 微信绿色
#define LIGHT_ASSISTANT_BUBBLE_COLOR lv_color_white()           // 白色
#define LIGHT_SYSTEM_BUBBLE_COLOR    lv_color_hex(0xE0E0E0)     // 浅灰色
#define LIGHT_SYSTEM_TEXT_COLOR      lv_color_hex(0x666666)     // 深灰色文本
#define LIGHT_BORDER_COLOR           lv_color_hex(0xE0E0E0)     // 浅灰色边框
#define LIGHT_LOW_BATTERY_COLOR      lv_color_black()           // 浅色模式下的黑色

// 主题颜色结构体
struct ThemeColors {
    lv_color_t background;      ///< 整体背景色 
    lv_color_t text;            ///< 普通文本颜色
    lv_color_t chat_background; ///< 聊天区背景色 
    lv_color_t user_bubble;     ///< 用户消息气泡背景色
    lv_color_t assistant_bubble;///< 机器人/助手消息气泡背景色
    lv_color_t system_bubble;   ///< 系统消息气泡背景色
    lv_color_t system_text;     ///< 系统消息的文字颜色
    lv_color_t border;          ///< 边框颜色，用于分隔容器、气泡等元素的细线描边 
    lv_color_t low_battery;     ///< 低电量警告色
};


// 深色主题配色方案
static const ThemeColors DARK_THEME = {
    .background = DARK_BACKGROUND_COLOR,
    .text = DARK_TEXT_COLOR,
    .chat_background = DARK_CHAT_BACKGROUND_COLOR,
    .user_bubble = DARK_USER_BUBBLE_COLOR,
    .assistant_bubble = DARK_ASSISTANT_BUBBLE_COLOR,
    .system_bubble = DARK_SYSTEM_BUBBLE_COLOR,
    .system_text = DARK_SYSTEM_TEXT_COLOR,
    .border = DARK_BORDER_COLOR,
    .low_battery = DARK_LOW_BATTERY_COLOR
};

// 浅色主题配色方案
static const ThemeColors LIGHT_THEME = {
    .background = LIGHT_BACKGROUND_COLOR,
    .text = LIGHT_TEXT_COLOR,
    .chat_background = LIGHT_CHAT_BACKGROUND_COLOR,
    .user_bubble = LIGHT_USER_BUBBLE_COLOR,
    .assistant_bubble = LIGHT_ASSISTANT_BUBBLE_COLOR,
    .system_bubble = LIGHT_SYSTEM_BUBBLE_COLOR,
    .system_text = LIGHT_SYSTEM_TEXT_COLOR,
    .border = LIGHT_BORDER_COLOR,
    .low_battery = LIGHT_LOW_BATTERY_COLOR
};

struct DisplayFonts {
    const lv_font_t* text_font = nullptr;
    const lv_font_t* icon_font = nullptr;
    const lv_font_t* emoji_font = nullptr;
};

enum class UIStyle {
    WeChat = 0, 
    GIF, 
    Minimal,
    Custom
};
#endif // THEME_FONTS_H