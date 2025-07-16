#include "lcd_display.h"

#include <vector>
#include <font_awesome_symbols.h>
#include <esp_log.h>
#include <esp_err.h>
#include <esp_lvgl_port.h>
#include "assets/lang_config.h"
#include <cstring>
#include "settings.h"

#include "board.h"

#include <libs/gif/lv_gif.h>

#define TAG "LcdDisplay"

#if CONFIG_USE_GIF_EMOTION_STYLE
LV_IMG_DECLARE(staticstate);
LV_IMG_DECLARE(sad);
LV_IMG_DECLARE(happy);
LV_IMG_DECLARE(scare);
LV_IMG_DECLARE(buxue);
LV_IMG_DECLARE(anger);
LV_IMG_DECLARE(silly);
#endif

// Current theme - initialize based on default config
static ThemeColors current_theme = LIGHT_THEME;

LV_FONT_DECLARE(font_awesome_30_4);

SpiLcdDisplay::SpiLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                           int width, int height, int offset_x, int offset_y, bool mirror_x, bool mirror_y, bool swap_xy,
                           DisplayFonts fonts)
    : LcdDisplay(panel_io, panel, fonts) {
    width_ = width;
    height_ = height;

    // draw white
    std::vector<uint16_t> buffer(width_, 0xFFFF);
    for (int y = 0; y < height_; y++) {
        esp_lcd_panel_draw_bitmap(panel_, 0, y, width_, y + 1, buffer.data());
    }

    // Set the display to on
    ESP_LOGI(TAG, "Turning display on");
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();

    ESP_LOGI(TAG, "Initialize LVGL port");
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority = 1;
    port_cfg.timer_period_ms = 50;
    lvgl_port_init(&port_cfg);

    ESP_LOGI(TAG, "Adding LCD screen");
    const lvgl_port_display_cfg_t display_cfg = {
        .io_handle = panel_io_,
        .panel_handle = panel_,
        .control_handle = nullptr,
        .buffer_size = static_cast<uint32_t>(width_ * 10),
        .double_buffer = false,
        .trans_size = 0,
        .hres = static_cast<uint32_t>(width_),
        .vres = static_cast<uint32_t>(height_),
        .monochrome = false,
        .rotation = {
            .swap_xy = swap_xy,
            .mirror_x = mirror_x,
            .mirror_y = mirror_y,
        },
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags = {
            .buff_dma = 1,
            .buff_spiram = 0,
            .sw_rotate = 0,
            .swap_bytes = 1,
            .full_refresh = 0,
            .direct_mode = 0,
        },
    };

    display_ = lvgl_port_add_disp(&display_cfg);
    if (display_ == nullptr) {
        ESP_LOGE(TAG, "Failed to add display");
        return;
    }

    if (offset_x != 0 || offset_y != 0) {
        lv_display_set_offset(display_, offset_x, offset_y);
    }

    // Update the theme
    if (current_theme_name_ == "dark") {
        current_theme = DARK_THEME;
    } else if (current_theme_name_ == "light") {
        current_theme = LIGHT_THEME;
    }

    SetupUI();
}

// RGB LCD实现
RgbLcdDisplay::RgbLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                           int width, int height, int offset_x, int offset_y,
                           bool mirror_x, bool mirror_y, bool swap_xy,
                           DisplayFonts fonts)
    : LcdDisplay(panel_io, panel, fonts) {
    width_ = width;
    height_ = height;
    
    // draw white
    std::vector<uint16_t> buffer(width_, 0xFFFF);
    for (int y = 0; y < height_; y++) {
        esp_lcd_panel_draw_bitmap(panel_, 0, y, width_, y + 1, buffer.data());
    }

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();

    ESP_LOGI(TAG, "Initialize LVGL port");
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority = 1;
    port_cfg.timer_period_ms = 50;
    lvgl_port_init(&port_cfg);

    ESP_LOGI(TAG, "Adding LCD screen");
    const lvgl_port_display_cfg_t display_cfg = {
        .io_handle = panel_io_,
        .panel_handle = panel_,
        .buffer_size = static_cast<uint32_t>(width_ * 10),
        .double_buffer = true,
        .hres = static_cast<uint32_t>(width_),
        .vres = static_cast<uint32_t>(height_),
        .rotation = {
            .swap_xy = swap_xy,
            .mirror_x = mirror_x,
            .mirror_y = mirror_y,
        },
        .flags = {
            .buff_dma = 1,
            .swap_bytes = 0,
            .full_refresh = 1,
            .direct_mode = 1,
        },
    };

    const lvgl_port_display_rgb_cfg_t rgb_cfg = {
        .flags = {
            .bb_mode = true,
            .avoid_tearing = true,
        }
    };
    
    display_ = lvgl_port_add_disp_rgb(&display_cfg, &rgb_cfg);
    if (display_ == nullptr) {
        ESP_LOGE(TAG, "Failed to add RGB display");
        return;
    }
    
    if (offset_x != 0 || offset_y != 0) {
        lv_display_set_offset(display_, offset_x, offset_y);
    }

    // Update the theme
    if (current_theme_name_ == "dark") {
        current_theme = DARK_THEME;
    } else if (current_theme_name_ == "light") {
        current_theme = LIGHT_THEME;
    }
    SetupUI();
}

LcdDisplay::LcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, DisplayFonts fonts)
    : panel_io_(panel_io), panel_(panel), fonts_(fonts) {
    }

LcdDisplay::~LcdDisplay()
{
    
}

bool LcdDisplay::Lock(int timeout_ms) 
{
    return lvgl_port_lock(timeout_ms);
}

void LcdDisplay::Unlock() 
{
    lvgl_port_unlock();
}


void LcdDisplay::SetupUI() 
{
    DisplayLockGuard lock(this);
    // 1. 获取默认屏幕并设置整体背景/文字字体
    auto screen = lv_screen_active();
    // 彻底清空屏幕的边距，避免留白
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_set_style_pad_hor(screen, 0, 0);
    lv_obj_set_style_pad_ver(screen, 0, 0);
    lv_obj_set_style_text_font(screen, fonts_.text_font, 0);
    lv_obj_set_style_text_color(screen, current_theme.text, 0);
    lv_obj_set_style_bg_color(screen, current_theme.background, 0);
    ESP_LOGI(TAG, "UI setup initiated. Screen resolution: %dx%d", (int)LV_HOR_RES, (int)LV_VER_RES); // 垂直分辨率

    // 2. 创建根容器，垂直布局：导航栏 + 内容区
    container_ = lv_obj_create(screen);
    lv_obj_set_size(container_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(container_, 0, 0);
    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(container_, 0, 0);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_pad_row(container_, 0, 0);
    lv_obj_set_style_bg_color(container_, current_theme.background, 0);
    lv_obj_set_style_border_color(container_, current_theme.border, 0);

    // 3. 创建并布局导航栏
    m_navBar = std::make_shared<NavigationBar>(fonts_);
    m_navBar->create(container_, [this](){
        // 设置按钮点击回调 —— 切换到“设置”页面
        //m_pageMgr->switchToPage("settings"); 
       // 弹出设置提示框
        // static const char* btns[] = {"确定", ""};
        // auto mbox = lv_msgbox_create(nullptr, nullptr);
        // lv_msgbox_set_text(mbox, "这里是设置提示框");
        // lv_msgbox_add_btns(mbox, btns);
        // lv_obj_center(mbox);
        ESP_LOGW(TAG, "Settings button clicked");
    });



    // 导航栏固定高度，自适应宽度
    //lv_obj_set_flex(m_navBar->getContainer(), LV_FLEX_WRAP, 0);
    //lv_obj_set_flex_grow(m_navBar->getContainer(), 0);
        // 确保导航栏不 grow，不占用剩余空间
    // lv_obj_set_flex_grow(m_navBar->getContainer(), 0);
    // // 绝对贴到根容器顶部
    // lv_obj_set_pos(m_navBar->getContainer(), 0, 0);
    // 4. 创建内容区容器，让它填满剩余空间
    content_ = lv_obj_create(container_);
    lv_obj_set_flex_grow(content_, 0);
    lv_obj_set_style_bg_color(content_, current_theme.chat_background, 0);
    lv_obj_set_style_border_color(content_, current_theme.border, 0);


    // 5. 初始化页面管理器，注册所有页面
    m_pageMgr = std::make_unique<PageManager>();
    //m_pageMgr->registerPage("wechat", std::make_unique<WeChatPage>());
    //m_pageMgr->registerPage("music", std::make_unique<MusicPage>());
    m_pageMgr->registerPage("weather", std::make_unique<WeatherPage>());
    // （如有“settings”页面可在此注册）
    m_pageMgr->init(content_, current_theme);

    // 6. 初始化屏幕事件管理：左右滑动翻页 + 导航栏点击
    m_screenEventMgr = std::make_unique<ScreenEventManager>(
        container_,
        // 左滑：下一页
        [this]()
        { 
            m_pageMgr->next(); 
            ESP_LOGD(TAG, "Next page");
        },
        // 右滑：上一页
        [this]()
        { 
            m_pageMgr->prev(); 
            ESP_LOGW(TAG, "Previous page");
        },
        // 导航栏“设置”点击：切换设置页
        [this]()
        { 
            //m_pageMgr->switchToPage("settings"); 
            ESP_LOGW(TAG, "Settings button clicked");
        });
    m_screenEventMgr->init(*m_navBar);
}

void LcdDisplay::RegisterPages()
{
    // auto wechatPage = std::make_shared<WeChatPage>();
    // auto musicPage = std::make_shared<MusicPage>();
    // m_pageMgr->RegisterPage("wechat", wechatPage);
    // m_pageMgr->RegisterPage("music", musicPage);
    // m_pageMgr->SwitchToPage("wechat");
}

void LcdDisplay::SetEmotion(const char* emotion) {
    struct Emotion {
        const char* icon;
        const char* text;
    };

    static const std::vector<Emotion> emotions = {
        {"😶", "neutral"},
        {"🙂", "happy"},
        {"😆", "laughing"},
        {"😂", "funny"},
        {"😔", "sad"},
        {"😠", "angry"},
        {"😭", "crying"},
        {"😍", "loving"},
        {"😳", "embarrassed"},
        {"😯", "surprised"},
        {"😱", "shocked"},
        {"🤔", "thinking"},
        {"😉", "winking"},
        {"😎", "cool"},
        {"😌", "relaxed"},
        {"🤤", "delicious"},
        {"😘", "kissy"},
        {"😏", "confident"},
        {"😴", "sleepy"},
        {"😜", "silly"},
        {"🙄", "confused"}
    };
    
    // 查找匹配的表情
    std::string_view emotion_view(emotion);
    auto it = std::find_if(emotions.begin(), emotions.end(),
        [&emotion_view](const Emotion& e) { return e.text == emotion_view; });

    DisplayLockGuard lock(this);
    if (emotion_label_ == nullptr) {
        return;
    }

    // 如果找到匹配的表情就显示对应图标，否则显示默认的neutral表情
    lv_obj_set_style_text_font(emotion_label_, fonts_.emoji_font, 0);
    if (it != emotions.end()) {
        lv_label_set_text(emotion_label_, it->icon);
    } else {
        lv_label_set_text(emotion_label_, "😶");
    }
    //转发
}

void LcdDisplay::SetIcon(const char* icon) {
    DisplayLockGuard lock(this);
    if (emotion_label_ == nullptr) {
        return;
    }
    lv_obj_set_style_text_font(emotion_label_, &font_awesome_30_4, 0);
    lv_label_set_text(emotion_label_, icon);
    // 转发
    
}

void LcdDisplay::SetTheme(const std::string& theme_name) {
    DisplayLockGuard lock(this);
    // 更新当前主题（保持原逻辑）
    if (theme_name == "dark" || theme_name == "DARK") {
        current_theme = DARK_THEME;
    } else if (theme_name == "light" || theme_name == "LIGHT") {
        current_theme = LIGHT_THEME;
    } else {
        ESP_LOGE(TAG, "Invalid theme name: %s", theme_name.c_str());
        return;
    }

  
    Display::SetTheme(theme_name);
}

void LcdDisplay::SetUIStyle(UIStyle ui_style)
{
    m_ui_style = ui_style;
}
