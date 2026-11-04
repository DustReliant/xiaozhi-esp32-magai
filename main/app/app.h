#ifndef MAGAI_APP_H
#define MAGAI_APP_H

#include <string>

#include "display/display.h"

/**
 * @brief magai 小智 App 基类
 *
 * 所有独立应用(天气时钟/2048/在线音乐/启动器)继承此类。
 * App 的 UI 建在覆盖层上(见 AppManager),Enter/Exit 由框架持锁调用,
 * 其他回调里操作 LVGL 必须自行加 DisplayLockGuard。
 */
class App {
public:
    enum ButtonId {
        kButtonTouch,     // GPIO0 触摸键
        kButtonVolumeUp,  // GPIO1 音量+
        kButtonVolumeDown // GPIO43 音量-
    };

    App(const std::string& name) : name_(name) {}
    virtual ~App() = default;

    const std::string& name() const { return name_; }

    /**
     * @brief 进入应用,构建 UI。调用方(AppManager)已持有 DisplayLockGuard。
     * @param parent 覆盖层父对象(全屏 lv_obj),App 的所有控件挂在其下
     */
    virtual void Enter(lv_obj_t* parent) = 0;

    /** @brief 退出应用,清理自建控件(框架会删除整个覆盖层,通常无需手动删)。已持锁。 */
    virtual void Exit() {}

    /** @brief 每秒时钟回调(未持锁)。操作 UI 需自行加 DisplayLockGuard。 */
    virtual void OnClockTick() {}

    /**
     * @brief 按键事件(未持锁)。操作 UI 需自行加 DisplayLockGuard。
     * @return true 已消费该事件;false 交回系统默认处理
     */
    virtual bool OnButton(ButtonId id, bool long_press) { return false; }

private:
    std::string name_;
};

#endif // MAGAI_APP_H
