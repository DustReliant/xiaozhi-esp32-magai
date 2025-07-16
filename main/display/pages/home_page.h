#ifndef HOME_PAGE_H
#define HOME_PAGE_H

#include "../page_manager.h"

// 主界面页面类
class HomePage : public BasePage {
public:
    HomePage(lv_obj_t* parent, Display* display, DisplayFonts fonts);
    ~HomePage() override = default;
    
    void Create() override;
    void Show() override;
    void Hide() override;
    void Update() override;
    bool HandleEvent(lv_event_t* e) override;
    
    // 更新界面元素
    void UpdateTime();
    void UpdateDeviceInfo();

private:
    // UI元素
    lv_obj_t* ai_icon_ = nullptr;
    lv_obj_t* device_info_label_ = nullptr;
    lv_obj_t* time_label_ = nullptr;
    
    // 定时器
    lv_timer_t* time_update_timer_ = nullptr;
    
    // 私有方法
    void CreateAIIcon();
    void CreateDeviceInfo();
    void CreateTimeDisplay();
    void SetupTimer();
    void ApplyTheme();
    
    // 静态回调函数
    static void TimeUpdateCallback(lv_timer_t* timer);
};

#endif // HOME_PAGE_H 