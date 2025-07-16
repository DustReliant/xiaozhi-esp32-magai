#include "ScreenEventManager.h"
#include "PageManager.h"
#include "NavigationBar.h"

ScreenEventManager::ScreenEventManager(lv_obj_t* parent,
                                       std::function<void()> onSwipeLeft,
                                       std::function<void()> onSwipeRight,
                                       std::function<void()> onNavClicked)
    : parent_(parent)
    , swipe_left_cb_(std::move(onSwipeLeft))
    , swipe_right_cb_(std::move(onSwipeRight))
    , nav_clicked_cb_(std::move(onNavClicked))
{}

void ScreenEventManager::init(NavigationBar& nav) {

    // 1) 绑定手势事件到 parent_（左右滑动）
    lv_obj_add_event_cb(parent_, gesture_cb, LV_EVENT_GESTURE, this);

    // 2) 绑定导航栏“设置”按钮点击事件
    //    假设 NavigationBar 已在 parent_ 下创建，并且按钮附加了一个已知的用户数据指针
    //    这里通过 lv_obj_get_child 或者在 NavigationBar 创建时暴露按钮句柄

    lv_obj_t* btn = nav.GetNavSettingsButton();
    if (btn) {
        lv_obj_add_event_cb(btn, nav_click_cb, LV_EVENT_CLICKED, this);
    }
}

// 静态回调：当发生手势事件时调用
void ScreenEventManager::gesture_cb(lv_event_t* e) {
    if(lv_event_get_code(e) != LV_EVENT_GESTURE) return;

    auto* mgr = static_cast<ScreenEventManager*>(lv_event_get_user_data(e));
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if(dir == LV_DIR_LEFT && mgr->swipe_left_cb_) {
        mgr->swipe_left_cb_();
    }
    else if(dir == LV_DIR_RIGHT && mgr->swipe_right_cb_) {
        mgr->swipe_right_cb_();
    }
}

// 静态回调：当导航按钮被点击时调用
void ScreenEventManager::nav_click_cb(lv_event_t* e) {
    auto* mgr = static_cast<ScreenEventManager*>(lv_event_get_user_data(e));
    if(mgr->nav_clicked_cb_) {
        mgr->nav_clicked_cb_();
    }
}
