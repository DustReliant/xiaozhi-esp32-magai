#ifndef MAGAI_APP_MANAGER_H
#define MAGAI_APP_MANAGER_H

#include <memory>
#include <string>
#include <vector>

#include "app.h"
#include "display/display.h"

/**
 * @brief App 管理器:App 注册表 + 当前活跃 App + 启动器切换
 *
 * 设计要点:
 * - 聊天界面是"桌面",App 以全屏覆盖层形式盖在聊天 UI 之上
 * - 按键事件先路由给活跃 App,未消费且无活跃 App 时走系统默认行为
 * - 时钟 tick 来自 Application 的 MAIN_EVENT_CLOCK_TICK 分发
 * - SwitchTo(nullptr) 回到启动器,启动器选"聊天"则关闭覆盖层回聊天
 */
class AppManager {
public:
    enum MainEvent {
        MAIN_EVENT_APP_NONE = 0,
    };

    static AppManager& GetInstance() {
        static AppManager instance;
        return instance;
    }

    /** 注册 App(建议在板卡构造后、Application::Start 前调用)。不接管所有权。 */
    void RegisterApp(App* app);

    /** 初始��:注册内置启动器并挂到 Application 生命周期。网络就绪后调用。 */
    void Initialize();

    /** 打开 App 启动器菜单(聊天态长按 vol+ 或语音触发)。 */
    void OpenLauncher();

    /** 切换到指定 App;nullptr 表示关闭覆盖层回到聊天。 */
    void SwitchTo(App* app);

    /** 启动器选中"聊天"或所有 App 退出时调用,回到聊天界面。 */
    void BackToChat();

    /**
     * @brief 按键路由入口(在 Button 回调上下文调用,未持锁)
     * @return true 事件被活跃 App/启动器消费,板级代码不应再处理
     */
    bool HandleButton(App::ButtonId id, bool long_press);

    /** 每秒 tick 分发(在主循环上下文,未持锁) */
    void OnClockTick();

    App* current_app() const { return current_app_; }
    bool launcher_open() const { return launcher_open_; }

    // 供启动器查询已注册的应用列表
    const std::vector<App*>& apps() const { return apps_; }

    /** 按名称查找(语音 MCP 工具用) */
    App* FindApp(const std::string& name);

private:
    AppManager() = default;

    void CreateOverlay();
    void DestroyOverlay();

    std::vector<App*> apps_;
    App* current_app_ = nullptr;  // 当前活跃 App(启动器打开时为 nullptr)
    bool launcher_open_ = false;
    lv_obj_t* overlay_ = nullptr; // 全屏覆盖层
};

#endif // MAGAI_APP_MANAGER_H
