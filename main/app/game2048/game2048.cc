#include "game2048/game2048.h"

#include <esp_log.h>
#include <esp_random.h>

#include "app_manager.h"
#include "board.h"
#include "display/lcd_display.h"
#include "display/lvgl_display/lvgl_theme.h"
#include "settings.h"

#define TAG "Game2048"

// 设计规范(docs/ui/DESIGN.md)色板
static constexpr uint32_t kColorCard     = 0x252836; // 卡片/棋盘底
static constexpr uint32_t kColorTextMain = 0xE8EAF0; // 主文字
static constexpr uint32_t kColorTextSub  = 0x9AA0B4; // 次文字
static constexpr uint32_t kColorTileEmpty = 0x3A3A42; // 空格

// 网格几何:240x280 屏,格 48 间隙 6,棋盘居中(含棋盘内边距 3)
static constexpr int kGridSize = 4;
static constexpr int kCell = 48;
static constexpr int kGap = 6;
static constexpr int kBoardPad = 3; // 棋盘容器内边距
static constexpr int kBoardInner = kGridSize * kCell + (kGridSize - 1) * kGap; // 210
static constexpr int kBoardSize = kBoardInner + 2 * kBoardPad;                 // 216
static constexpr int kBoardX = (240 - kBoardSize) / 2; // 12
static constexpr int kBoardY = 56;
// 4 个格子起点(棋盘容器坐标系内)
static constexpr int kCellX(int c) { return kBoardPad + c * (kCell + kGap); }
static constexpr int kCellY(int r) { return kBoardPad + r * (kCell + kGap); }

// 滑动方向判定阈值(px)
static constexpr int kSwipeThreshold = 30;

void Game2048App::Enter(lv_obj_t* parent) {
    auto display = Board::GetInstance().GetDisplay();
    auto lvgl_theme = static_cast<LvglTheme*>(display->GetTheme());
    auto text_font = lvgl_theme->text_font()->font();

    Settings settings("game2048", true);
    best_ = settings.GetInt("best", 0);

    /* 顶部:分数卡(左 104x32)/最佳卡(右 104x32),#252836 圆角 8 */
    score_card_ = lv_obj_create(parent);
    lv_obj_set_pos(score_card_, 12, 16);
    lv_obj_set_size(score_card_, 104, 32);
    lv_obj_set_style_radius(score_card_, 8, 0);
    lv_obj_set_style_bg_color(score_card_, lv_color_hex(kColorCard), 0);
    lv_obj_set_style_bg_opa(score_card_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(score_card_, 0, 0);
    lv_obj_set_style_pad_all(score_card_, 0, 0);
    lv_obj_clear_flag(score_card_, LV_OBJ_FLAG_SCROLLABLE);

    score_label_ = lv_label_create(score_card_);
    lv_obj_set_style_text_font(score_label_, text_font, 0);
    lv_obj_set_style_text_color(score_label_, lv_color_hex(kColorTextMain), 0);
    lv_obj_center(score_label_);
    lv_label_set_text(score_label_, "分数 0");

    best_card_ = lv_obj_create(parent);
    lv_obj_set_pos(best_card_, 240 - 12 - 104, 16);
    lv_obj_set_size(best_card_, 104, 32);
    lv_obj_set_style_radius(best_card_, 8, 0);
    lv_obj_set_style_bg_color(best_card_, lv_color_hex(kColorCard), 0);
    lv_obj_set_style_bg_opa(best_card_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(best_card_, 0, 0);
    lv_obj_set_style_pad_all(best_card_, 0, 0);
    lv_obj_clear_flag(best_card_, LV_OBJ_FLAG_SCROLLABLE);

    best_label_ = lv_label_create(best_card_);
    lv_obj_set_style_text_font(best_label_, text_font, 0);
    lv_obj_set_style_text_color(best_label_, lv_color_hex(kColorTextMain), 0);
    lv_obj_center(best_label_);
    lv_label_set_text_fmt(best_label_, "最佳 %d", best_);

    /* 棋盘容器:#252836 圆角 12,接收触摸手势 */
    board_cont_ = lv_obj_create(parent);
    lv_obj_set_pos(board_cont_, kBoardX, kBoardY);
    lv_obj_set_size(board_cont_, kBoardSize, kBoardSize);
    lv_obj_set_style_radius(board_cont_, 12, 0);
    lv_obj_set_style_bg_color(board_cont_, lv_color_hex(kColorCard), 0);
    lv_obj_set_style_bg_opa(board_cont_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(board_cont_, 0, 0);
    lv_obj_set_style_pad_all(board_cont_, 0, 0);
    lv_obj_clear_flag(board_cont_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(board_cont_, LV_OBJ_FLAG_CLICKABLE);

    /* 16 个格子 */
    for (int r = 0; r < kGridSize; r++) {
        for (int c = 0; c < kGridSize; c++) {
            tiles_[r][c] = lv_obj_create(board_cont_);
            lv_obj_set_pos(tiles_[r][c], kCellX(c), kCellY(r));
            lv_obj_set_size(tiles_[r][c], kCell, kCell);
            lv_obj_set_style_radius(tiles_[r][c], 8, 0);
            lv_obj_set_style_bg_color(tiles_[r][c], lv_color_hex(kColorTileEmpty), 0);
            lv_obj_set_style_bg_opa(tiles_[r][c], LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(tiles_[r][c], 0, 0);
            lv_obj_set_style_pad_all(tiles_[r][c], 0, 0);
            lv_obj_clear_flag(tiles_[r][c], LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_clear_flag(tiles_[r][c], LV_OBJ_FLAG_CLICKABLE); // 命中交给棋盘手势

            tiles_label_[r][c] = lv_label_create(tiles_[r][c]);
            lv_obj_set_style_text_font(tiles_label_[r][c], text_font, 0);
            lv_obj_center(tiles_label_[r][c]);
        }
    }

    /* 触摸滑动手势:PRESSED 记起点,RELEASED 算位移方向。
     * 格子已去掉 CLICKABLE,命中对象只可能是:棋盘容器/分数卡/结束层/覆盖层,
     * 四处都挂上,保证任意起手的滑动都能被捕获。 */
    auto gesture_cb = [](lv_event_t* e) {
        auto* app = static_cast<Game2048App*>(lv_event_get_user_data(e));
        lv_indev_t* indev = lv_indev_active();
        if (indev == nullptr) {
            return;
        }
        lv_point_t point;
        lv_indev_get_point(indev, &point);
        app->OnGesture(lv_event_get_code(e), point.x, point.y);
    };
    lv_obj_add_event_cb(board_cont_, gesture_cb, LV_EVENT_PRESSED, this);
    lv_obj_add_event_cb(board_cont_, gesture_cb, LV_EVENT_RELEASED, this);
    lv_obj_add_event_cb(score_card_, gesture_cb, LV_EVENT_PRESSED, this);
    lv_obj_add_event_cb(score_card_, gesture_cb, LV_EVENT_RELEASED, this);
    lv_obj_add_event_cb(best_card_, gesture_cb, LV_EVENT_PRESSED, this);
    lv_obj_add_event_cb(best_card_, gesture_cb, LV_EVENT_RELEASED, this);
    lv_obj_add_event_cb(parent, gesture_cb, LV_EVENT_PRESSED, this);
    lv_obj_add_event_cb(parent, gesture_cb, LV_EVENT_RELEASED, this);

    /* 游戏结束提示层(盖在棋盘上方,挡住下层手势;自身挂手势回调=点按重开) */
    overlay_msg_ = lv_obj_create(parent);
    lv_obj_set_pos(overlay_msg_, 20, 140);
    lv_obj_set_size(overlay_msg_, 200, 60);
    lv_obj_set_style_radius(overlay_msg_, 10, 0);
    lv_obj_set_style_bg_color(overlay_msg_, lv_color_hex(0x1A1B25), 0);
    lv_obj_set_style_bg_opa(overlay_msg_, LV_OPA_80, 0);
    lv_obj_set_style_border_width(overlay_msg_, 0, 0);
    lv_obj_set_style_pad_all(overlay_msg_, 0, 0);
    auto msg = lv_label_create(overlay_msg_);
    lv_obj_set_style_text_font(msg, text_font, 0);
    lv_obj_set_style_text_color(msg, lv_color_hex(kColorTextMain), 0);
    lv_obj_center(msg);
    lv_label_set_text(msg, "游戏结束\n触摸屏幕重开");
    lv_obj_add_flag(overlay_msg_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(overlay_msg_, gesture_cb, LV_EVENT_PRESSED, this);
    lv_obj_add_event_cb(overlay_msg_, gesture_cb, LV_EVENT_RELEASED, this);

    NewGame();
}

void Game2048App::OnGesture(uint32_t event_code, int x, int y) {
    // 在 LVGL 事件回调里执行,动 UI 前需持锁(递归锁,重入安全)
    auto display = Board::GetInstance().GetDisplay();
    DisplayLockGuard lock(display);
    if (event_code == LV_EVENT_PRESSED) {
        gesture_start_x_ = x;
        gesture_start_y_ = y;
        gesture_active_ = true;
        return;
    }
    if (event_code != LV_EVENT_RELEASED || !gesture_active_) {
        return;
    }
    gesture_active_ = false;
    if (game_over_) {
        NewGame(); // 游戏结束:任意触摸重开
        return;
    }
    int dx = x - gesture_start_x_;
    int dy = y - gesture_start_y_;
    if (dx * dx + dy * dy < kSwipeThreshold * kSwipeThreshold) {
        return; // 位移过小,不判定方向
    }
    Direction dir;
    if (dx * dx > dy * dy) {
        dir = (dx > 0) ? kRight : kLeft;
    } else {
        dir = (dy > 0) ? kDown : kUp;
    }
    HandleMove(dir);
}

void Game2048App::HandleMove(Direction dir) {
    // 统一移动入口:物理键与触摸手势共用。调用方需持锁(或经 OnGesture 已持锁)
    if (game_over_) {
        return;
    }
    if (Move(dir)) {
        SpawnTile();
        UpdateUi();
        if (IsGameOver()) {
            game_over_ = true;
            if (overlay_msg_ != nullptr) {
                lv_obj_clear_flag(overlay_msg_, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
}

void Game2048App::Exit() {
    score_card_ = nullptr;
    best_card_ = nullptr;
    board_cont_ = nullptr;
    score_label_ = nullptr;
    best_label_ = nullptr;
    overlay_msg_ = nullptr;
    gesture_active_ = false;
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            tiles_[r][c] = nullptr;
            tiles_label_[r][c] = nullptr;
        }
    }
}

bool Game2048App::OnButton(ButtonId id, bool long_press) {
    if (id == kButtonTouch && long_press) {
        AppManager::GetInstance().BackToChat();
        return true;
    }
    if (game_over_) {
        if (id == kButtonTouch && !long_press) {
            NewGame();
            return true;
        }
        return true; // 游戏结束吃掉其余按键
    }
    if (id == kButtonTouch && !long_press) {
        NewGame();
        return true;
    }
    Direction dir = kNone;
    if (id == kButtonVolumeUp && !long_press) dir = kUp;
    if (id == kButtonVolumeUp && long_press) dir = kRight;
    if (id == kButtonVolumeDown && !long_press) dir = kDown;
    if (id == kButtonVolumeDown && long_press) dir = kLeft;
    if (dir != kNone) {
        // 未持锁回调进入,HandleMove 内动 UI 需持锁
        auto display = Board::GetInstance().GetDisplay();
        DisplayLockGuard lock(display);
        HandleMove(dir);
    }
    return true; // 游戏内所有方向键均消费
}

void Game2048App::NewGame() {
    for (auto& row : board_) {
        for (int& v : row) v = 0;
    }
    score_ = 0;
    game_over_ = false;
    SpawnTile();
    SpawnTile();
    auto display = Board::GetInstance().GetDisplay();
    DisplayLockGuard lock(display);
    if (overlay_msg_ != nullptr) {
        lv_obj_add_flag(overlay_msg_, LV_OBJ_FLAG_HIDDEN);
    }
    UpdateUi();
}

bool Game2048App::Move(Direction dir) {
    bool moved = false;

    // 把每条线按方向取出来,统一按"向索引 0 滑动"处理
    for (int line = 0; line < kGridSize; line++) {
        int vals[kGridSize];
        for (int i = 0; i < kGridSize; i++) {
            switch (dir) {
                case kLeft:  vals[i] = board_[line][i]; break;
                case kRight: vals[i] = board_[line][kGridSize - 1 - i]; break;
                case kUp:    vals[i] = board_[i][line]; break;
                case kDown:  vals[i] = board_[kGridSize - 1 - i][line]; break;
                default: break;
            }
        }

        // 1) 压缩非零到前部
        int nums[kGridSize] = {0};
        int n = 0;
        for (int i = 0; i < kGridSize; i++) {
            if (vals[i] > 0) nums[n++] = vals[i];
        }
        // 2) 相邻同值合并(每格只合并一次)
        for (int i = 0; i + 1 < n; i++) {
            if (nums[i] == nums[i + 1]) {
                nums[i] *= 2;
                score_ += nums[i];
                for (int j = i + 1; j + 1 < n; j++) {
                    nums[j] = nums[j + 1];
                }
                n--;
            }
        }
        for (int i = n; i < kGridSize; i++) nums[i] = 0;

        // 3) 写回并比较是否变化
        for (int i = 0; i < kGridSize; i++) {
            int new_v = nums[i];
            int old_v = 0;
            switch (dir) {
                case kLeft:  old_v = board_[line][i]; board_[line][i] = new_v; break;
                case kRight: old_v = board_[line][kGridSize - 1 - i]; board_[line][kGridSize - 1 - i] = new_v; break;
                case kUp:    old_v = board_[i][line]; board_[i][line] = new_v; break;
                case kDown:  old_v = board_[kGridSize - 1 - i][line]; board_[kGridSize - 1 - i][line] = new_v; break;
                default: break;
            }
            if (old_v != new_v) moved = true;
        }
    }
    return moved;
}

void Game2048App::SpawnTile() {
    // 收集空格
    int empty[16][2];
    int count = 0;
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (board_[r][c] == 0) {
                empty[count][0] = r;
                empty[count][1] = c;
                count++;
            }
        }
    }
    if (count == 0) return;
    int idx = esp_random() % count;
    int value = (esp_random() % 10 < 9) ? 2 : 4;
    board_[empty[idx][0]][empty[idx][1]] = value;
}

bool Game2048App::IsGameOver() {
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (board_[r][c] == 0) return false;
            if (c < 3 && board_[r][c] == board_[r][c + 1]) return false;
            if (r < 3 && board_[r][c] == board_[r + 1][c]) return false;
        }
    }
    return true;
}

void Game2048App::UpdateUi() {
    // 调用方持锁或已持锁场景;保险起见自己加锁(递归锁,重入安全)
    auto display = Board::GetInstance().GetDisplay();
    DisplayLockGuard lock(display);
    if (score_label_ == nullptr) return;

    char buf[32];
    snprintf(buf, sizeof(buf), "分数 %d", score_);
    lv_label_set_text(score_label_, buf);
    if (score_ > best_) {
        best_ = score_;
        Settings settings("game2048", true);
        settings.SetInt("best", best_);
    }
    lv_label_set_text_fmt(best_label_, "最佳 %d", best_);

    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            int v = board_[r][c];
            if (v == 0) {
                lv_label_set_text(tiles_label_[r][c], "");
                lv_obj_set_style_bg_color(tiles_[r][c], lv_color_hex(kColorTileEmpty), 0);
            } else {
                lv_label_set_text_fmt(tiles_label_[r][c], "%d", v);
                lv_obj_set_style_bg_color(tiles_[r][c], lv_color_hex(TileColor(v)), 0);
                lv_obj_set_style_text_color(tiles_label_[r][c],
                    v <= 4 ? lv_color_hex(0x3A3A42) : lv_color_hex(0xFFFFFF), 0);
            }
        }
    }
}

uint32_t Game2048App::TileColor(int value) {
    switch (value) {
        case 2:    return 0xEEE4DA;
        case 4:    return 0xEDE0C8;
        case 8:    return 0xF2B179;
        case 16:   return 0xF59563;
        case 32:   return 0xF67C5F;
        case 64:   return 0xF65E3B;
        case 128:  return 0xEDCF72;
        case 256:  return 0xEDCC61;
        case 512:  return 0xEDC850;
        case 1024: return 0xEDC53F;
        case 2048: return 0xEDC22E;
        default:   return 0xEDC22E;
    }
}
