#ifndef MAGAI_GAME2048_H
#define MAGAI_GAME2048_H

#include <cstdint>

#include "app/app.h"

/**
 * @brief 2048 游戏 App
 *
 * 4x4 网格。触摸:上下左右滑动=移动方向,游戏结束后点按=重开;
 * 实体键:vol+ 短/长=上/右,vol- 短/长=下/左,touch 短按=新游戏,touch 长按=退出。
 * 移动统一走 HandleMove(),物理键与触摸手势共用。
 */
class Game2048App : public App {
public:
    static Game2048App& GetInstance() {
        static Game2048App instance;
        return instance;
    }

    void Enter(lv_obj_t* parent) override;
    void Exit() override;
    bool OnButton(ButtonId id, bool long_press) override;

private:
    Game2048App() : App("2048") {}

    enum Direction { kNone, kUp, kDown, kLeft, kRight };

    void NewGame();
    bool Move(Direction dir);      // 返回是否有移动
    void SpawnTile();
    bool IsGameOver();
    void UpdateUi();               // 需持锁
    void HandleMove(Direction dir); // 统一移动入口(移动+落子+结束判定)
    void OnGesture(uint32_t event_code, int x, int y); // 触摸手势状态机

    static uint32_t TileColor(int value);

    int board_[4][4] = {{0}};
    int score_ = 0;
    int best_ = 0;
    bool game_over_ = false;

    // 触摸手势状态
    bool gesture_active_ = false;
    int gesture_start_x_ = 0;
    int gesture_start_y_ = 0;

    lv_obj_t* score_card_ = nullptr;
    lv_obj_t* best_card_ = nullptr;
    lv_obj_t* board_cont_ = nullptr;
    lv_obj_t* score_label_ = nullptr;
    lv_obj_t* best_label_ = nullptr;
    lv_obj_t* tiles_[4][4] = {{nullptr}};
    lv_obj_t* tiles_label_[4][4] = {{nullptr}};
    lv_obj_t* overlay_msg_ = nullptr;
};

#endif // MAGAI_GAME2048_H
