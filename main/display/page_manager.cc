#include "page_manager.h"
#include "pages/home_page.h"
#include "pages/music_page.h"
#include <esp_log.h>

#define TAG "PageManager"

PageManager::PageManager(Display* display, DisplayFonts fonts) : display_(display), fonts_(fonts) {
    SetupStatusBar();
    SetupPages();
}

PageManager::~PageManager() {
    // 清理页面
    for (auto& pair : pages_) {
        delete pair.second;
    }
    pages_.clear();
    
    // 清理UI对象
    if (main_container_) {
        lv_obj_del(main_container_);
    }
}

void PageManager::SetupStatusBar() {
    auto screen = lv_screen_active();
    
    // 创建状态栏
    status_bar_ = lv_obj_create(screen);
    lv_obj_set_size(status_bar_, 240, 32);
    lv_obj_set_pos(status_bar_, 0, 0);
    lv_obj_set_style_radius(status_bar_, 0, 0);
    lv_obj_set_style_border_width(status_bar_, 0, 0);
    lv_obj_set_style_pad_all(status_bar_, 0, 0);
    
    // 设置flex布局
    lv_obj_set_flex_flow(status_bar_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(status_bar_, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    // AI芯片图标
    ai_icon_ = lv_label_create(status_bar_);
    lv_label_set_text(ai_icon_, "🤖");
    lv_obj_set_style_text_font(ai_icon_, fonts_.text_font ? fonts_.text_font : &lv_font_montserrat_14, 0);
    lv_obj_set_style_margin_left(ai_icon_, 8, 0);
    
    // 网络状态图标
    network_icon_ = lv_label_create(status_bar_);
    lv_label_set_text(network_icon_, "📶");
    lv_obj_set_style_text_font(network_icon_, fonts_.text_font ? fonts_.text_font : &lv_font_montserrat_14, 0);
    lv_obj_set_style_margin_left(network_icon_, 4, 0);
    
    // 通知区域 (中央)
    notification_area_ = lv_label_create(status_bar_);
    lv_label_set_text(notification_area_, "正在初始化...");
    lv_obj_set_style_text_font(notification_area_, fonts_.text_font ? fonts_.text_font : &lv_font_montserrat_14, 0);
    lv_obj_set_flex_grow(notification_area_, 1);
    lv_obj_set_style_text_align(notification_area_, LV_TEXT_ALIGN_CENTER, 0);
    
    // 静音图标
    mute_icon_ = lv_label_create(status_bar_);
    lv_label_set_text(mute_icon_, "🔊");
    lv_obj_set_style_text_font(mute_icon_, fonts_.text_font ? fonts_.text_font : &lv_font_montserrat_14, 0);
    lv_obj_set_style_margin_right(mute_icon_, 4, 0);
    
    // 电池图标
    battery_icon_ = lv_label_create(status_bar_);
    lv_label_set_text(battery_icon_, "🔋");
    lv_obj_set_style_text_font(battery_icon_, fonts_.text_font ? fonts_.text_font : &lv_font_montserrat_14, 0);
    lv_obj_set_style_margin_right(battery_icon_, 4, 0);
    
    // 设置按钮
    settings_btn_ = lv_btn_create(status_bar_);
    lv_obj_set_size(settings_btn_, 20, 18);
    lv_obj_set_style_bg_color(settings_btn_, lv_color_hex(0x6c757d), 0);
    lv_obj_set_style_radius(settings_btn_, 4, 0);
    lv_obj_set_style_margin_right(settings_btn_, 8, 0);
    lv_obj_add_event_cb(settings_btn_, OnSettingsButtonClicked, LV_EVENT_CLICKED, this);
    
    lv_obj_t* settings_label = lv_label_create(settings_btn_);
    lv_label_set_text(settings_label, "⚙️");
    lv_obj_set_style_text_color(settings_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(settings_label, fonts_.text_font ? fonts_.text_font : &lv_font_montserrat_14, 0);
    lv_obj_center(settings_label);
    
    // 创建主容器 (状态栏下方)
    main_container_ = lv_obj_create(screen);
    lv_obj_set_size(main_container_, 240, 248); // 280 - 32 (状态栏)
    lv_obj_set_pos(main_container_, 0, 32);
    lv_obj_set_style_radius(main_container_, 0, 0);
    lv_obj_set_style_border_width(main_container_, 0, 0);
    lv_obj_set_style_pad_all(main_container_, 0, 0);
    
    ApplyTheme();
}

void PageManager::SetupPages() {
    // 创建主界面页面
    pages_[PageType::HOME] = new HomePage(main_container_, display_, fonts_);
    
    // 创建音乐播放页面
    pages_[PageType::MUSIC] = new MusicPage(main_container_, display_, fonts_);
    
    // 其他页面暂时用空实现，后续添加
    // pages_[PageType::CHAT] = new ChatPage(main_container_, display_);
    // pages_[PageType::SETTINGS] = new SettingsPage(main_container_, display_);
    // pages_[PageType::WEATHER] = new WeatherPage(main_container_, display_);
    // pages_[PageType::TIME] = new TimePage(main_container_, display_);
    
    // 默认显示主界面
    SwitchToPage(PageType::HOME, false);
}

void PageManager::SwitchToPage(PageType page_type, bool animate) {
    ESP_LOGI(TAG, "Switching to page: %d", static_cast<int>(page_type));
    
    // 隐藏当前页面
    if (pages_.find(current_page_) != pages_.end()) {
        pages_[current_page_]->Hide();
    }
    
    // 显示新页面
    if (pages_.find(page_type) != pages_.end()) {
        pages_[page_type]->Show();
        current_page_ = page_type;
        
        // 添加到历史记录
        PushToHistory(page_type);
        
        // 更新状态栏显示
        UpdateStatusBar();
    } else {
        ESP_LOGE(TAG, "Page not found: %d", static_cast<int>(page_type));
    }
}

void PageManager::GoBack() {
    if (page_history_.size() > 1) {
        // 移除当前页面
        page_history_.pop_back();
        // 获取上一个页面
        PageType prev_page = page_history_.back();
        page_history_.pop_back(); // 再次移除，因为SwitchToPage会重新添加
        
        SwitchToPage(prev_page, true);
    } else {
        // 没有历史记录，返回主界面
        SwitchToPage(PageType::HOME, true);
    }
}

BasePage* PageManager::GetCurrentPageObject() const {
    auto it = pages_.find(current_page_);
    return (it != pages_.end()) ? it->second.get() : nullptr;
}

void PageManager::PushToHistory(PageType page_type) {
    // 避免重复添加相同页面
    if (page_history_.empty() || page_history_.back() != page_type) {
        page_history_.push_back(page_type);
        
        // 限制历史记录长度
        if (page_history_.size() > 10) {
            page_history_.erase(page_history_.begin());
        }
    }
}

void PageManager::ClearHistory() {
    page_history_.clear();
}

bool PageManager::HandleEvent(lv_event_t* e) {
    // 首先尝试让当前页面处理事件
    BasePage* current_page = GetCurrentPageObject();
    if (current_page && current_page->HandleEvent(e)) {
        return true;
    }
    
    // 页面管理器自身的事件处理
    // 可以在这里添加全局快捷键等
    
    return false;
}

void PageManager::UpdateStatusBar() {
    if (notification_area_) {
        switch (current_page_) {
            case PageType::HOME:
                lv_label_set_text(notification_area_, "正在初始化...");
                break;
            case PageType::MUSIC:
                lv_label_set_text(notification_area_, "音乐播放");
                break;
            case PageType::CHAT:
                lv_label_set_text(notification_area_, "AI助手");
                break;
            case PageType::SETTINGS:
                lv_label_set_text(notification_area_, "设置");
                break;
            case PageType::WEATHER:
                lv_label_set_text(notification_area_, "天气");
                break;
            case PageType::TIME:
                lv_label_set_text(notification_area_, "时间");
                break;
            default:
                lv_label_set_text(notification_area_, "");
                break;
        }
    }
}

void PageManager::ApplyTheme() {
    if (!status_bar_ || !main_container_) return;
    
    std::string theme = display_->GetTheme();
    
    if (theme == "dark") {
        // 深色主题
        lv_obj_set_style_bg_color(status_bar_, lv_color_hex(0x121212), 0);
        lv_obj_set_style_bg_color(main_container_, lv_color_hex(0x121212), 0);
        
        lv_obj_set_style_text_color(ai_icon_, lv_color_white(), 0);
        lv_obj_set_style_text_color(network_icon_, lv_color_white(), 0);
        lv_obj_set_style_text_color(notification_area_, lv_color_white(), 0);
        lv_obj_set_style_text_color(mute_icon_, lv_color_white(), 0);
        lv_obj_set_style_text_color(battery_icon_, lv_color_white(), 0);
    } else {
        // 浅色主题
        lv_obj_set_style_bg_color(status_bar_, lv_color_hex(0xe9ecef), 0);
        lv_obj_set_style_bg_color(main_container_, lv_color_white(), 0);
        
        lv_obj_set_style_text_color(ai_icon_, lv_color_black(), 0);
        lv_obj_set_style_text_color(network_icon_, lv_color_black(), 0);
        lv_obj_set_style_text_color(notification_area_, lv_color_black(), 0);
        lv_obj_set_style_text_color(mute_icon_, lv_color_black(), 0);
        lv_obj_set_style_text_color(battery_icon_, lv_color_black(), 0);
    }
    
    // 通知所有页面更新主题
    for (auto& [page_type, page] : pages_) {
        // 可以添加页面主题更新方法
        page->Update();
    }
}

void PageManager::OnSettingsButtonClicked(lv_event_t* e) {
    PageManager* manager = static_cast<PageManager*>(lv_event_get_user_data(e));
    ESP_LOGI(TAG, "Settings button clicked");
    
    // 切换到设置页面 (暂时切换到音乐页面作为演示)
    manager->SwitchToPage(PageType::MUSIC, true);
} 