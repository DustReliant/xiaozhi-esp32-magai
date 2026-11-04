#ifndef MAGAI_LAUNCHER_H
#define MAGAI_LAUNCHER_H

#include <vector>

#include "app/app.h"

/**
 * @brief App 启动器:全屏菜单,列出聊天 + 所有已注册 App
 *
 * 交互:vol+/- 上下导航,touch 短按进入选中项,touch 长按返回聊天
 * 启动器不进 AppManager::apps_ 列表,由 AppManager 直接调度
 */
class Launcher : public App {
public:
    static Launcher& GetInstance() {
        static Launcher instance;
        return instance;
    }

    void Enter(lv_obj_t* parent) override;
    void Exit() override;
    bool OnButton(ButtonId id, bool long_press) override;

private:
    Launcher() : App("launcher") {}

    void UpdateHighlight();

    lv_obj_t* title_label_ = nullptr;
    lv_obj_t* back_btn_ = nullptr;
    lv_obj_t* list_cont_ = nullptr;
    std::vector<lv_obj_t*> items_;  // 菜单项(含首项"聊天")
    int selected_ = 0;              // 0=聊天,1..N=apps_[i-1]
};

#endif // MAGAI_LAUNCHER_H
