#include "page_manager.h"
#include "pages/home_page.h"
#include "pages/music_page.h"
#include <esp_log.h>
#include <algorithm>

#define TAG "PageManager"

PageManager::PageManager(Display* display, DisplayFonts fonts) : display_(display), fonts_(fonts) {
    SetupStatusBar();
    SetupPages();
}

PageManager::~PageManager() {
    // 停止所有定时器
    if (page_manager_timer_) {
        lv_timer_del(page_manager_timer_);
        page_manager_timer_ = nullptr;
    }
    
    // 清理页面
    for (auto& pair : pages_) {
        if (pair.second) {
            // 调用页面的销毁前回调
            pair.second->OnDestroy();
            delete pair.second;
        }
    }
    pages_.clear();
    
    // 清理状态栏UI对象（按依赖关系反向清理）
    if (settings_btn_) {
        lv_obj_del(settings_btn_);
        settings_btn_ = nullptr;
    }
    if (battery_icon_) {
        lv_obj_del(battery_icon_);
        battery_icon_ = nullptr;
    }
    if (mute_icon_) {
        lv_obj_del(mute_icon_);
        mute_icon_ = nullptr;
    }
    if (notification_area_) {
        lv_obj_del(notification_area_);
        notification_area_ = nullptr;
    }
    if (network_icon_) {
        lv_obj_del(network_icon_);
        network_icon_ = nullptr;
    }
    if (ai_icon_) {
        lv_obj_del(ai_icon_);
        ai_icon_ = nullptr;
    }
    
    // 清理主容器（这会自动清理子对象）
    if (main_container_) {
        lv_obj_del(main_container_);
        main_container_ = nullptr;
    }
    
    // 最后清理状态栏
    if (status_bar_) {
        lv_obj_del(status_bar_);
        status_bar_ = nullptr;
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
    // 不再预先创建所有页面，改为懒加载模式
    // 仅清空页面映射，页面将在首次访问时创建
    pages_.clear();
    
    // 默认显示主界面
    SwitchToPage(PageType::HOME, false);
}

void PageManager::SwitchToPage(PageType page_type, bool animate) {
    ESP_LOGI(TAG, "Switching to page: %d", static_cast<int>(page_type));
    
    // 隐藏当前页面
    if (pages_.find(current_page_) != pages_.end() && pages_[current_page_]) {
        pages_[current_page_]->Hide();
    }
    
    // 检查目标页面是否已存在，如不存在则创建
    if (pages_.find(page_type) == pages_.end() || !pages_[page_type]) {
        CreatePage(page_type);
    } else {
        // 页面已存在，更新缓存顺序
        UpdatePageCacheOrder(page_type);
    }
    
    // 显示新页面
    if (pages_[page_type]) {
        pages_[page_type]->Show();
        current_page_ = page_type;
        
        // 添加到历史记录
        PushToHistory(page_type);
        
        // 更新状态栏显示
        UpdateStatusBar();
        
        ESP_LOGI(TAG, "Successfully switched to page: %d, cached pages: %zu", 
                 static_cast<int>(page_type), pages_.size());
    } else {
        ESP_LOGE(TAG, "Failed to create or find page: %d", static_cast<int>(page_type));
    }
}

void PageManager::CreatePage(PageType page_type) {
    if (!main_container_) {
        ESP_LOGE(TAG, "Main container not initialized");
        return;
    }
    
    // 如果页面已存在，更新缓存顺序
    if (pages_.find(page_type) != pages_.end() && pages_[page_type]) {
        UpdatePageCacheOrder(page_type);
        return;
    }
    
    // 检查是否需要清理缓存
    if (pages_.size() >= MAX_CACHED_PAGES) {
        CleanupOldestPage();
    }
    
    // 根据页面类型创建对应页面
    switch (page_type) {
        case PageType::HOME:
            pages_[page_type] = new HomePage(main_container_, display_, fonts_);
            ESP_LOGI(TAG, "Created HomePage");
            break;
            
        case PageType::MUSIC:
            pages_[page_type] = new MusicPage(main_container_, display_, fonts_);
            ESP_LOGI(TAG, "Created MusicPage");
            break;
            
        case PageType::CHAT:
            // pages_[page_type] = new ChatPage(main_container_, display_, fonts_);
            ESP_LOGW(TAG, "ChatPage not implemented yet");
            break;
            
        case PageType::SETTINGS:
            // pages_[page_type] = new SettingsPage(main_container_, display_, fonts_);
            ESP_LOGW(TAG, "SettingsPage not implemented yet");
            break;
            
        case PageType::WEATHER:
            // pages_[page_type] = new WeatherPage(main_container_, display_, fonts_);
            ESP_LOGW(TAG, "WeatherPage not implemented yet");
            break;
            
        case PageType::TIME:
            // pages_[page_type] = new TimePage(main_container_, display_, fonts_);
            ESP_LOGW(TAG, "TimePage not implemented yet");
            break;
            
        default:
            ESP_LOGE(TAG, "Unknown page type: %d", static_cast<int>(page_type));
            return;
    }
    
    // 更新缓存顺序
    if (pages_[page_type]) {
        UpdatePageCacheOrder(page_type);
    }
}

void PageManager::UpdatePageCacheOrder(PageType page_type) {
    // 移除已存在的记录
    auto it = std::find(page_cache_order_.begin(), page_cache_order_.end(), page_type);
    if (it != page_cache_order_.end()) {
        page_cache_order_.erase(it);
    }
    
    // 添加到最前面（最近使用）
    page_cache_order_.insert(page_cache_order_.begin(), page_type);
    
    ESP_LOGD(TAG, "Updated page cache order, current size: %zu", page_cache_order_.size());
}

void PageManager::CleanupOldestPage() {
    if (page_cache_order_.empty()) {
        return;
    }
    
    // 获取最久未使用的页面（列表末尾）
    PageType oldest_page = page_cache_order_.back();
    
    // 如果是当前页面，不能清理
    if (oldest_page == current_page_) {
        if (page_cache_order_.size() > 1) {
            oldest_page = page_cache_order_[page_cache_order_.size() - 2];
        } else {
            return; // 只有当前页面，不清理
        }
    }
    
    // 删除页面
    auto it = pages_.find(oldest_page);
    if (it != pages_.end() && it->second) {
        ESP_LOGI(TAG, "Cleaning up oldest page: %d", static_cast<int>(oldest_page));
        
        // 调用页面的销毁前回调
        it->second->OnDestroy();
        
        delete it->second;
        pages_.erase(it);
    }
    
    // 从缓存顺序中移除
    auto cache_it = std::find(page_cache_order_.begin(), page_cache_order_.end(), oldest_page);
    if (cache_it != page_cache_order_.end()) {
        page_cache_order_.erase(cache_it);
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
    return (it != pages_.end()) ? it->second : nullptr;
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
    
    // 应用状态栏主题
    if (theme == "dark") {
        // 深色主题
        lv_obj_set_style_bg_color(status_bar_, lv_color_hex(0x121212), 0);
        lv_obj_set_style_bg_color(main_container_, lv_color_hex(0x121212), 0);
        
        if (ai_icon_) lv_obj_set_style_text_color(ai_icon_, lv_color_white(), 0);
        if (network_icon_) lv_obj_set_style_text_color(network_icon_, lv_color_white(), 0);
        if (notification_area_) lv_obj_set_style_text_color(notification_area_, lv_color_white(), 0);
        if (mute_icon_) lv_obj_set_style_text_color(mute_icon_, lv_color_white(), 0);
        if (battery_icon_) lv_obj_set_style_text_color(battery_icon_, lv_color_white(), 0);
    } else {
        // 浅色主题
        lv_obj_set_style_bg_color(status_bar_, lv_color_hex(0xe9ecef), 0);
        lv_obj_set_style_bg_color(main_container_, lv_color_white(), 0);
        
        if (ai_icon_) lv_obj_set_style_text_color(ai_icon_, lv_color_black(), 0);
        if (network_icon_) lv_obj_set_style_text_color(network_icon_, lv_color_black(), 0);
        if (notification_area_) lv_obj_set_style_text_color(notification_area_, lv_color_black(), 0);
        if (mute_icon_) lv_obj_set_style_text_color(mute_icon_, lv_color_black(), 0);
        if (battery_icon_) lv_obj_set_style_text_color(battery_icon_, lv_color_black(), 0);
    }
    
    // 通知所有页面更新主题
    for (auto& [page_type, page] : pages_) {
        if (page) {
            page->Update();
        }
    }
    
    ESP_LOGI(TAG, "Applied theme: %s to %zu pages", theme.c_str(), pages_.size());
}

void PageManager::OnSettingsButtonClicked(lv_event_t* e) {
    PageManager* manager = static_cast<PageManager*>(lv_event_get_user_data(e));
    ESP_LOGI(TAG, "Settings button clicked");
    
    // 切换到设置页面 (暂时切换到音乐页面作为演示)
    manager->SwitchToPage(PageType::MUSIC, true);
}

// BasePage 事件处理辅助方法实现
void BasePage::AddEventHandler(lv_obj_t* obj, lv_event_cb_t cb, lv_event_code_t filter, void* user_data) {
    if (obj && cb) {
        lv_obj_add_event_cb(obj, cb, filter, user_data ? user_data : this);
    }
}

void BasePage::RemoveEventHandler(lv_obj_t* obj, lv_event_cb_t cb) {
    if (obj && cb) {
        lv_obj_remove_event_cb(obj, cb);
    }
} 