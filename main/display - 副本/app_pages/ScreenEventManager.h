#pragma once

#include <lvgl.h>
#include <functional>

class PageManager;
class NavigationBar;

class ScreenEventManager {
public:
    /// onSwipeLeft ：用户左滑时调用（切到下一页）
    /// onSwipeRight：用户右滑时调用（切到上一页）
    /// onNavClicked：点击导航栏“设置”按钮时调用
    ScreenEventManager(lv_obj_t* parent,
                       std::function<void()> onSwipeLeft,
                       std::function<void()> onSwipeRight,
                       std::function<void()> onNavClicked);

    /// 绑定手势和导航按钮事件
    void init(NavigationBar& nav);

private:
    static void gesture_cb(lv_event_t* e);
    static void nav_click_cb(lv_event_t* e);

    lv_obj_t* parent_;
    std::function<void()> swipe_left_cb_;
    std::function<void()> swipe_right_cb_;
    std::function<void()> nav_clicked_cb_;
};
