#include "home_page.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <time.h>
#include <sys/time.h>
#include "../../board.h"
#include "../../application.h"

#define TAG "HomePage"

HomePage::HomePage(lv_obj_t* parent, Display* display, DisplayFonts fonts) 
    : BasePage(parent, display, fonts) {
    Create();
}

void HomePage::Create() {
    // 创建主容器
    container_ = lv_obj_create(parent_);
    lv_obj_set_size(container_, LV_HOR_RES, LV_VER_RES - 32); // 减去状态栏高度
    lv_obj_set_pos(container_, 0, 32); // 状态栏下方
    lv_obj_set_style_pad_all(container_, 0, 0);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_radius(container_, 0, 0);
    
    // 创建界面元素
    CreateAIIcon();
    CreateDeviceInfo();
    CreateTimeDisplay();
    
    // 应用主题
    ApplyTheme();
    
    // 设置定时器
    SetupTimer();
    
    // 初始隐藏
    Hide();
}

void HomePage::CreateAIIcon() {
    ai_icon_ = lv_label_create(container_);
    lv_label_set_text(ai_icon_, "🤖");
    lv_obj_set_style_text_font(ai_icon_, fonts_.text_font ? fonts_.text_font : &lv_font_montserrat_14, 0);
    lv_obj_align(ai_icon_, LV_ALIGN_CENTER, 0, -40);
}

void HomePage::CreateDeviceInfo() {
    device_info_label_ = lv_label_create(container_);
    lv_obj_set_style_text_font(device_info_label_, fonts_.text_font ? fonts_.text_font : &lv_font_montserrat_14, 0);
    lv_obj_align(device_info_label_, LV_ALIGN_CENTER, 0, 20);
    lv_obj_set_style_text_align(device_info_label_, LV_TEXT_ALIGN_CENTER, 0);
    
    // 更新设备信息
    UpdateDeviceInfo();
}

void HomePage::CreateTimeDisplay() {
    time_label_ = lv_label_create(container_);
    lv_obj_set_style_text_font(time_label_, fonts_.text_font ? fonts_.text_font : &lv_font_montserrat_14, 0);
    lv_obj_align(time_label_, LV_ALIGN_CENTER, 0, 70);
    lv_obj_set_style_text_align(time_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(time_label_, lv_color_black(), 0);
    
    // 更新时间
    UpdateTime();
}

void HomePage::SetupTimer() {
    // 创建1秒更新一次的定时器
    time_update_timer_ = lv_timer_create(TimeUpdateCallback, 1000, this);
}

void HomePage::Show() {
    lv_obj_clear_flag(container_, LV_OBJ_FLAG_HIDDEN);
    UpdateTime();
    UpdateDeviceInfo();
}

void HomePage::Hide() {
    lv_obj_add_flag(container_, LV_OBJ_FLAG_HIDDEN);
}

void HomePage::Update() {
    UpdateTime();
    UpdateDeviceInfo();
}

bool HomePage::HandleEvent(lv_event_t* e) {
    // 处理主界面的特定事件
    return false;
}

void HomePage::UpdateTime() {
    if (time_label_ == nullptr) return;
    
    time_t now;
    struct tm timeinfo;
    char time_str[16];
    
    time(&now);
    localtime_r(&now, &timeinfo);
    
    // 格式化为 HH:MM
    strftime(time_str, sizeof(time_str), "%H:%M", &timeinfo);
    lv_label_set_text(time_label_, time_str);
}

void HomePage::UpdateDeviceInfo() {
    if (device_info_label_ == nullptr) return;
    
    // 获取设备信息
    std::string info = "v1.6.1";
    
    // 获取网络状态
    // 这里需要根据实际的网络模块获取状态
    if (Board::GetInstance().HasWiFi()) {
        info += " | WiFi已连接";
    } else {
        info += " | WiFi未连接";
    }
    
    lv_label_set_text(device_info_label_, info.c_str());
}

void HomePage::ApplyTheme() {
    if (container_ == nullptr) return;
    
    // 获取当前主题
    std::string theme = display_->GetTheme();
    
    if (theme == "dark") {
        lv_obj_set_style_bg_color(container_, lv_color_hex(0x121212), 0);
        lv_obj_set_style_text_color(ai_icon_, lv_color_white(), 0);
        lv_obj_set_style_text_color(device_info_label_, lv_color_hex(0x6c757d), 0);
        lv_obj_set_style_text_color(time_label_, lv_color_white(), 0);
    } else {
        lv_obj_set_style_bg_color(container_, lv_color_white(), 0);
        lv_obj_set_style_text_color(ai_icon_, lv_color_black(), 0);
        lv_obj_set_style_text_color(device_info_label_, lv_color_hex(0x6c757d), 0);
        lv_obj_set_style_text_color(time_label_, lv_color_black(), 0);
    }
}

void HomePage::TimeUpdateCallback(lv_timer_t* timer) {
    HomePage* page = static_cast<HomePage*>(timer->user_data);
    if (page != nullptr) {
        page->UpdateTime();
    }
} 