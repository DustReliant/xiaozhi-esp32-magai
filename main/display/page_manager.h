#ifndef PAGE_MANAGER_H
#define PAGE_MANAGER_H

#include <lvgl.h>
#include <functional>
#include <map>
#include <string>
#include "display.h"

// 页面类型枚举
enum class PageType {
    HOME,      // 主界面
    CHAT,      // 聊天页面
    SETTINGS,  // 设置页面
    WEATHER,   // 天气页面
    TIME,      // 时间显示页面
    MUSIC      // 音乐播放页面
};

// 页面基类
class BasePage {
public:
    BasePage(lv_obj_t* parent, Display* display, DisplayFonts fonts) : parent_(parent), display_(display), fonts_(fonts) {}
    virtual ~BasePage() = default;
    
    virtual void Create() = 0;
    virtual void Show() = 0;
    virtual void Hide() = 0;
    virtual void Update() {}
    virtual bool HandleEvent(lv_event_t* e) { return false; }
    virtual void OnDestroy() {}  // 销毁前回调
    
    lv_obj_t* GetContainer() const { return container_; }
    bool IsVisible() const { return container_ && !lv_obj_has_flag(container_, LV_OBJ_FLAG_HIDDEN); }

protected:
    lv_obj_t* parent_ = nullptr;
    lv_obj_t* container_ = nullptr;
    Display* display_ = nullptr;
    DisplayFonts fonts_;
    
    // 事件处理辅助方法（内联实现）
    void AddEventHandler(lv_obj_t* obj, lv_event_cb_t cb, lv_event_code_t filter, void* user_data = nullptr) {
        if (obj && cb) {
            lv_obj_add_event_cb(obj, cb, filter, user_data ? user_data : this);
        }
    }
    
    void RemoveEventHandler(lv_obj_t* obj, lv_event_cb_t cb) {
        if (obj && cb) {
            lv_obj_remove_event_cb(obj, cb);
        }
    }
};

// 页面管理器类
class PageManager {
public:
    PageManager(Display* display, DisplayFonts fonts);
    ~PageManager();
    
    // 页面管理
    void SetupPages();
    void SwitchToPage(PageType page_type, bool animate = true);
    void GoBack();
    
    // 当前页面信息
    PageType GetCurrentPage() const { return current_page_; }
    BasePage* GetCurrentPageObject() const;
    
    // 页面导航历史
    void PushToHistory(PageType page_type);
    void ClearHistory();
    
    // 事件处理
    bool HandleEvent(lv_event_t* e);
    
    // 状态栏管理
    void SetupStatusBar();
    void UpdateStatusBar();
    
    // 主题支持
    void ApplyTheme();

private:
    Display* display_ = nullptr;
    DisplayFonts fonts_;
    lv_obj_t* main_container_ = nullptr;
    lv_obj_t* status_bar_ = nullptr;
    
    // 页面管理
    std::map<PageType, BasePage*> pages_;
    PageType current_page_ = PageType::HOME;
    std::vector<PageType> page_history_;
    
    // 页面缓存管理
    static const size_t MAX_CACHED_PAGES = 3;  // 最大缓存页面数量
    std::vector<PageType> page_cache_order_;   // 页面缓存顺序（LRU）
    
    // 定时器
    lv_timer_t* page_manager_timer_ = nullptr;
    
    // 状态栏元素
    lv_obj_t* ai_icon_ = nullptr;
    lv_obj_t* network_icon_ = nullptr;
    lv_obj_t* notification_area_ = nullptr;
    lv_obj_t* mute_icon_ = nullptr;
    lv_obj_t* battery_icon_ = nullptr;
    lv_obj_t* settings_btn_ = nullptr;
    
    // 私有方法
    void CreateStatusBar();
    void SetupEventHandlers();
    void CreatePage(PageType page_type);
    void UpdatePageCacheOrder(PageType page_type);
    void CleanupOldestPage();
    static void OnSettingsButtonClicked(lv_event_t* e);
};

#endif // PAGE_MANAGER_H 